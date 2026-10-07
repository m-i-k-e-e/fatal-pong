PS5_HOST ?= ps5
PS5_PORT ?= 9021

PS5_PAYLOAD_SDK ?= /opt/ps5-payload-sdk
include $(PS5_PAYLOAD_SDK)/toolchain/prospero.mk

VERSION     ?= 1.0.0
# Homebrew folder name on the PS5 and release file names
APP_NAME    := fatal-pong

# Object files and converted sounds go in target/; target/install/ mirrors the homebrew folder on the PS5
BUILD_DIR   := target
INSTALL_DIR := $(BUILD_DIR)/install
DIST_DIR    := $(BUILD_DIR)/dist
TARGET      := $(INSTALL_DIR)/eboot.elf
ICON        := $(INSTALL_DIR)/sce_sys/icon0.png
SOUNDS      := $(BUILD_DIR)/hadouken.wav $(BUILD_DIR)/tennis-ball.wav \
               $(BUILD_DIR)/agassi-wins.wav $(BUILD_DIR)/nadal-wins.wav \
               $(BUILD_DIR)/graf-wins.wav $(BUILD_DIR)/sharapova-wins.wav \
               $(BUILD_DIR)/finish-him.wav $(BUILD_DIR)/finish-her.wav $(BUILD_DIR)/fatality.wav \
               $(BUILD_DIR)/cartoon-boomerang.wav $(BUILD_DIR)/fatality-scream.wav
GRUNTS      := $(BUILD_DIR)/grunt.wav $(BUILD_DIR)/kasplat-grunt.wav
SOUNDS      += $(GRUNTS)

# Use the SDK's prospero toolchain and its bundled SDL2 (not a local copy of the headers).
# EMBED_DIR is where audio.c .incbin's the converted sounds from.
CFLAGS   := -O2 -Wall $(shell $(PKG_CONFIG) --cflags sdl2) -DEMBED_DIR=\"$(CURDIR)/$(BUILD_DIR)/\"
LDLIBS   := $(shell $(PKG_CONFIG) --static --libs sdl2) -lm

SRCS     := main.c audio.c ball.c bonus.c calamity.c draw.c fatality.c fireball.c hud.c paddle.c particles.c pause.c players.c rift.c text.c
OBJS     := $(SRCS:%.c=$(BUILD_DIR)/%.o)

all: $(TARGET) $(ICON)

$(TARGET): $(OBJS) | $(INSTALL_DIR)
	$(CC) $(OBJS) -o $@ $(LDLIBS)

$(BUILD_DIR)/%.o: %.c $(wildcard *.h) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/audio.o: $(SOUNDS)
$(BUILD_DIR)/players.o: player_sprites.inc
$(BUILD_DIR)/bonus.o: bonus_icons.inc

$(ICON): assets/icon0.png
	mkdir -p $(@D)
	cp $< $@

$(BUILD_DIR) $(INSTALL_DIR):
	mkdir -p $@

# SDL core only decodes WAV, so convert clips to 48 kHz mono 16-bit (the audio device format)
# and trim trailing silence so they don't hold a mixer voice
$(BUILD_DIR)/%.wav: %.mp3 | $(BUILD_DIR)
	ffmpeg -y -loglevel error -i $< -ac 1 -ar 48000 -c:a pcm_s16le \
		-af "areverse,silenceremove=start_periods=1:start_threshold=-60dB,areverse" $@

# Grunts also lose their leading silence, so they land with the hit
$(GRUNTS): $(BUILD_DIR)/%.wav: %.mp3 | $(BUILD_DIR)
	ffmpeg -y -loglevel error -i $< -ac 1 -ar 48000 -c:a pcm_s16le \
		-af "silenceremove=start_periods=1:start_threshold=-50dB,areverse,silenceremove=start_periods=1:start_threshold=-60dB,areverse" $@

clean:
	rm -rf $(BUILD_DIR)

# Send the bare payload to elfldr
test: $(TARGET)
	$(PS5_DEPLOY) -h $(PS5_HOST) -p $(PS5_PORT) $^

# Home screen installer (installer.c): a payload carrying the game, its icon and the tile's metadata, which
# installs a Fatal Pong tile; the tile starts the game through websrv
INSTALLER   := $(BUILD_DIR)/$(APP_NAME)-installer.elf
SHORTCUT    := assets/shortcut/param.json assets/shortcut/launch.html

$(INSTALLER): installer.c $(TARGET) assets/icon0.png $(SHORTCUT) | $(BUILD_DIR)
	$(CC) -O2 -Wall -DEMBED_EBOOT=\"$(CURDIR)/$(TARGET)\" -DEMBED_ICON=\"$(CURDIR)/assets/icon0.png\" \
		-DEMBED_PARAM=\"$(CURDIR)/assets/shortcut/param.json\" -DEMBED_LAUNCH=\"$(CURDIR)/assets/shortcut/launch.html\" \
		$< -o $@ -lSceIpmi -lSceAppInstUtil

installer: $(INSTALLER)

# Send the installer to elfldr: adds (or updates) the Fatal Pong tile on the home screen
install-shortcut: $(INSTALLER)
	$(PS5_DEPLOY) -h $(PS5_HOST) -p $(PS5_PORT) $^

# Upload as a homebrew app for websrv's launcher (needs ftpsrv.elf running on the PS5)
PS5_FTP_PORT ?= 2121
HB_DIR       := /data/homebrew/$(APP_NAME)

install: all
	curl --ftp-create-dirs -T $(TARGET) ftp://$(PS5_HOST):$(PS5_FTP_PORT)$(HB_DIR)/eboot.elf
	curl --ftp-create-dirs -T $(ICON) ftp://$(PS5_HOST):$(PS5_FTP_PORT)$(HB_DIR)/sce_sys/icon0.png

# Release files: a zip of the homebrew folder for the launcher, the same executable as a bare payload, and the
# home screen installer
dist: all $(INSTALLER) assets/README.txt
	rm -rf $(DIST_DIR)
	mkdir -p $(DIST_DIR)/$(APP_NAME)
	cp -r $(INSTALL_DIR)/. $(DIST_DIR)/$(APP_NAME)/
	cp assets/README.txt $(DIST_DIR)/$(APP_NAME)/
	cd $(DIST_DIR) && zip -qr $(APP_NAME)-$(VERSION).zip $(APP_NAME)
	cp $(TARGET) $(DIST_DIR)/$(APP_NAME)-$(VERSION).elf
	cp $(INSTALLER) $(DIST_DIR)/$(APP_NAME)-installer-$(VERSION).elf
	rm -rf $(DIST_DIR)/$(APP_NAME)
	@ls -l $(DIST_DIR)

# Promo screenshots (assets/screenshots/start, pause, win, mole, earthquake, frog-rain, finale .png and hadouken, rift, mole,
# earthquake, frog-rain, win, fatality, finale .gif), rendered by tools/screenshots.c
# with the real game code, built for this machine: needs a native compiler, SDL2 dev files and ffmpeg
HOST_CC      ?= cc
HOST_SDL      = $(shell pkg-config --cflags --libs sdl2)
SHOT_DIR     := assets/screenshots
FRAMES_DIR   := $(BUILD_DIR)/frames
SHOT_SRCS    := tools/screenshots.c audio.c ball.c bonus.c draw.c fatality.c hud.c paddle.c particles.c pause.c players.c rift.c text.c

$(BUILD_DIR)/screenshots: $(SHOT_SRCS) fireball.c calamity.c player_sprites.inc bonus_icons.inc $(wildcard *.h) $(SOUNDS) | $(BUILD_DIR)
	$(HOST_CC) -O2 -Wall -I. -DEMBED_DIR=\"$(CURDIR)/$(BUILD_DIR)/\" $(SHOT_SRCS) -o $@ $(HOST_SDL) -lm

screenshots: $(BUILD_DIR)/screenshots
	rm -rf $(FRAMES_DIR)
	mkdir -p $(FRAMES_DIR) $(SHOT_DIR)
	$(BUILD_DIR)/screenshots
	ffmpeg -y -loglevel error -i $(FRAMES_DIR)/start.bmp $(SHOT_DIR)/start.png
	ffmpeg -y -loglevel error -i $(FRAMES_DIR)/pause.bmp $(SHOT_DIR)/pause.png
	ffmpeg -y -loglevel error -i $(FRAMES_DIR)/win.bmp $(SHOT_DIR)/win.png
	ffmpeg -y -loglevel error -i $(FRAMES_DIR)/finale.bmp $(SHOT_DIR)/finale.png
	ffmpeg -y -loglevel error -i $(FRAMES_DIR)/mole.bmp $(SHOT_DIR)/mole.png
	ffmpeg -y -loglevel error -i $(FRAMES_DIR)/earthquake.bmp $(SHOT_DIR)/earthquake.png
	ffmpeg -y -loglevel error -i $(FRAMES_DIR)/frog-rain.bmp $(SHOT_DIR)/frog-rain.png
	ffmpeg -y -loglevel error -framerate 30 -i $(FRAMES_DIR)/hadouken_%03d.bmp \
		-vf "scale=960:-1:flags=neighbor,split[a][b];[a]palettegen=max_colors=128[p];[b][p]paletteuse=dither=none" \
		$(SHOT_DIR)/hadouken.gif
	ffmpeg -y -loglevel error -framerate 30 -i $(FRAMES_DIR)/rift_%03d.bmp \
		-vf "scale=960:-1:flags=neighbor,split[a][b];[a]palettegen=max_colors=128[p];[b][p]paletteuse=dither=none" \
		$(SHOT_DIR)/rift.gif
	ffmpeg -y -loglevel error -framerate 20 -i $(FRAMES_DIR)/mole_%03d.bmp \
		-vf "scale=960:-1:flags=neighbor,split[a][b];[a]palettegen=max_colors=128[p];[b][p]paletteuse=dither=none" \
		$(SHOT_DIR)/mole.gif
	# The tremors move every pixel of the noisy grass, which a GIF can't compress: smaller, sparser frames
	ffmpeg -y -loglevel error -framerate 20 -i $(FRAMES_DIR)/earthquake_%03d.bmp \
		-vf "fps=10,scale=720:-1:flags=neighbor,split[a][b];[a]palettegen=max_colors=64[p];[b][p]paletteuse=dither=none" \
		$(SHOT_DIR)/earthquake.gif
	ffmpeg -y -loglevel error -framerate 20 -i $(FRAMES_DIR)/frog-rain_%03d.bmp \
		-vf "scale=960:-1:flags=neighbor,split[a][b];[a]palettegen=max_colors=128[p];[b][p]paletteuse=dither=none" \
		$(SHOT_DIR)/frog-rain.gif
	ffmpeg -y -loglevel error -framerate 30 -i $(FRAMES_DIR)/win_%03d.bmp \
		-vf "scale=960:-1:flags=neighbor,split[a][b];[a]palettegen=max_colors=128[p];[b][p]paletteuse=dither=none" \
		$(SHOT_DIR)/win.gif
	ffmpeg -y -loglevel error -framerate 20 -i $(FRAMES_DIR)/fatality_%03d.bmp \
		-vf "scale=960:-1:flags=neighbor,split[a][b];[a]palettegen=max_colors=128[p];[b][p]paletteuse=dither=none" \
		$(SHOT_DIR)/fatality.gif
	ffmpeg -y -loglevel error -framerate 20 -i $(FRAMES_DIR)/finale_%03d.bmp \
		-vf "scale=960:-1:flags=neighbor,split[a][b];[a]palettegen=max_colors=128[p];[b][p]paletteuse=dither=none" \
		$(SHOT_DIR)/finale.gif
	rm -rf $(FRAMES_DIR)
	@ls -l $(SHOT_DIR)

.PHONY: all clean test install installer install-shortcut dist screenshots
