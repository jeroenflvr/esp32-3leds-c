.PHONY: load-sdk build flash monitor clean fullclean menuconfig erase-flash reconfigure upload-spiffs create-env

SHELL := /bin/zsh
PORT ?= /dev/cu.usbmodem101
# Ensure the ESP-IDF environment is loaded before any idf.py call
IDF_ACTIVATE ?= $(HOME)/.espressif/tools/activate_idf_v6.1.sh
IDF = command -v idf.py >/dev/null || source $(IDF_ACTIVATE) >/dev/null 2>&1; idf.py

load-sdk:
	@echo "Loading SDK..."
	# Add commands to load the SDK here
	$(IDF) set-target esp32c6

build: 
	@echo "Building project..."
	$(IDF) build

flash: upload-spiffs
	@echo "Flashing project to device..."
	$(IDF) -p $(PORT) flash

monitor:
	@echo "Starting monitor..."
	$(IDF) -p $(PORT) monitor

flash-monitor: upload-spiffs flash
	@echo "Flashing and monitoring..."
	$(IDF) -p $(PORT) monitor

clean:
	@echo "Cleaning build artifacts..."
	$(IDF) clean

fullclean:
	@echo "Performing full clean (removes build directory)..."
	$(IDF) fullclean

reconfigure:
	@echo "Reconfiguring project..."
	$(IDF) reconfigure

menuconfig:
	@echo "Opening configuration menu..."
	$(IDF) menuconfig

erase-flash:
	@echo "Erasing flash memory..."
	$(IDF) -p $(PORT) erase-flash

# Utility targets
size:
	@echo "Analyzing binary size..."
	$(IDF) size

app-flash:
	@echo "Flashing only the app (faster)..."
	$(IDF) -p $(PORT) app-flash

# Combined workflow targets
rebuild: clean build

reset: fullclean build

provision: erase-flash flash-monitor

# SPIFFS targets for development mode
upload-spiffs:
	@echo "Creating SPIFFS image and uploading to device..."
	@if [ ! -d "data" ]; then \
		echo "Error: data/ directory not found. Create it and add your .env file."; \
		exit 1; \
	fi
	@source $(IDF_ACTIVATE) >/dev/null 2>&1; \
	python $$IDF_PATH/components/spiffs/spiffsgen.py 0x10000 data build/spiffs.bin && \
	python -m esptool --chip esp32c6 --port $(PORT) write-flash 0x190000 build/spiffs.bin

create-env:
	@echo "Creating .env file from template..."
	@if [ -f "data/.env" ]; then \
		echo "data/.env already exists. Remove it first if you want to recreate."; \
	else \
		cp data/.env.example data/.env; \
		echo "Created data/.env from template. Edit it with your configuration."; \
	fi