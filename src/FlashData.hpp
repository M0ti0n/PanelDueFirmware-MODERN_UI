#ifndef FLASH_DATA
#define FLASH_DATA 1

#include <cstdint>

#include "Hardware/Backlight.hpp"
#include "UI/MessageLog.hpp"
#include "General/SimpleMath.h"
#include "UI/Display.hpp"

enum class DisplayDimmerType : uint8_t
{
	never = 0,				// never dim the display
	onIdle, 				// only display when printer status is idle
	always,					// default - always dim
	NumTypes
};

enum class HeaterCombineType : uint8_t
{
	notCombined = 0,
	combined,
	NumTypes
};

struct FlashData;

extern FlashData nvData, savedNvData;


struct FlashData
{
	// The magic value should be changed whenever the layout of the NVRAM changes
	// We now use a different magic value for each display size, to force the "touch the spot" screen to be displayed when you change the display size
	static const uint32_t magicVal = 0x3AB64A60 + DISPLAY_TYPE;
	static const uint32_t muggleVal = 0xFFFFFFFF;

	alignas(4) uint32_t magic;
	uint32_t baudRate;
	uint16_t xmin;
	uint16_t xmax;
	uint16_t ymin;
	uint16_t ymax;
	DisplayOrientation lcdOrientation;
	DisplayOrientation touchOrientation;
	uint8_t touchVolume;
	uint8_t language;
	uint8_t colourScheme;
	uint8_t brightness;
	DisplayDimmerType displayDimmerType;
	uint8_t infoTimeout;
	uint32_t screensaverTimeout;
	uint8_t babystepAmountIndex;
	uint16_t feedrate;
	HeaterCombineType heaterCombineType;
	MessageLog::LogLevel logLevel;
	uint8_t accentColour;								// index of the modern UI Accent colour, 0..7
	// FILAMENT MANAGEMENT. Added after the older fields so existing settings stay valid. Stored as value + 1 (see Encode):
	// 0x00 and 0xFF both mean "not set".
	uint8_t filamentMode;								// 1 = filament macros (RRF), 2 = panel only, else not chosen yet
	uint8_t slotColour[16];								// per tool/slot: colour palette index + 1 (1..64), else none
	uint8_t slotMaterial[16];							// per tool/slot: built-in material index + 1 (panel mode), else none
	alignas(4) char dummy;								// must be at a multiple of 4 bytes from the start because flash is read/written in whole dwords

	FlashData() : magic(muggleVal) { SetDefaults(); }
	bool operator==(const FlashData& other);
	bool operator!=(const FlashData& other) { return !operator==(other); }
	bool IsValid() const;
	void SetInvalid() { magic = muggleVal; }
	void SetDefaults();
	void Load();
	void Save() const;

	bool IsSaveNeeded();

	bool SetColourScheme(uint8_t newColours);
	void SetInfoTimeout(uint8_t newInfoTimeout) { nvData.infoTimeout = newInfoTimeout; }
	bool SetLanguage(uint8_t newLanguage);

	void SetDisplayDimmerType(DisplayDimmerType newType) { nvData.displayDimmerType = newType; }
	DisplayDimmerType GetDisplayDimmerType() { return nvData.displayDimmerType; }

	void SetVolume(uint8_t newVolume) { nvData.touchVolume = newVolume; }
	uint32_t GetVolume() { return nvData.touchVolume; }

	void SetScreensaverTimeout(uint32_t screensaverTimeout) { nvData.screensaverTimeout = screensaverTimeout; }
	uint32_t GetScreensaverTimeout() { return nvData.screensaverTimeout; }

	void SetBabystepAmountIndex(uint8_t babystepAmountIndex) { nvData.babystepAmountIndex = babystepAmountIndex; }
	uint8_t GetBabystepAmountIndex() { return nvData.babystepAmountIndex; }

	void SetFeedrate(uint16_t feedrate) { nvData.feedrate = feedrate; }
	uint16_t GetFeedrate() { return nvData.feedrate; }

	void SetHeaterCombineType(HeaterCombineType combine) { nvData.heaterCombineType = combine; }
	HeaterCombineType GetHeaterCombineType() { return nvData.heaterCombineType; }

	void SetLogLevel(MessageLog::LogLevel logLevel) { nvData.logLevel = logLevel; }
	MessageLog::LogLevel GetLogLevel() { return nvData.logLevel; }

	void SetAccentColour(uint8_t newAccent) { nvData.accentColour = newAccent; }
	uint8_t GetAccentColour() { return nvData.accentColour; }

	// The three FILAMENT MANAGEMENT values are stored as value + 1, so both 0x00 and 0xFF read back as "not set".
	// Older firmware writes the user signature from a zero-filled page buffer, so bytes after its settings read as 0x00,
	// while a never-written signature reads as 0xFF.
	static constexpr unsigned int FilamentSlots = 16;
	static uint8_t Encode(uint8_t v) { return (v == 0xFF) ? 0 : (uint8_t)(v + 1); }
	static uint8_t Decode(uint8_t raw, uint8_t limit) { return (raw != 0 && raw <= limit) ? (uint8_t)(raw - 1) : 0xFF; }
	uint8_t GetFilamentMode() { return Decode(nvData.filamentMode, 2); }
	void SetFilamentMode(uint8_t mode) { nvData.filamentMode = Encode(mode); }
	uint8_t GetSlotColour(size_t slot) { return (slot < FilamentSlots) ? Decode(nvData.slotColour[slot], 64) : 0xFF; }
	void SetSlotColour(size_t slot, uint8_t colour) { if (slot < FilamentSlots) { nvData.slotColour[slot] = Encode(colour); } }
	uint8_t GetSlotMaterial(size_t slot) { return (slot < FilamentSlots) ? Decode(nvData.slotMaterial[slot], 254) : 0xFF; }
	void SetSlotMaterial(size_t slot, uint8_t material) { if (slot < FilamentSlots) { nvData.slotMaterial[slot] = Encode(material); } }

	void SetBaudRate(uint32_t rate) { nvData.baudRate = rate; }
	uint32_t GetBaudRate() { return nvData.baudRate; }

	void SetBrightness(uint32_t percent) { nvData.brightness =
		constrain<int>(percent, Backlight::MinBrightness, Backlight::MaxBrightness); }
	int GetBrightness() { return (int)nvData.brightness; }


};

#endif /* ifndef FLASH_DATA */
