/*
 * UserInterface.cpp
 *
 *  Created on: 7 Jan 2017
 *      Author: David
 */

#include <UI/UserInterface.hpp>

#include <ctype.h>

#include "Configuration.hpp"
#include "FileManager.hpp"
#include "FlashData.hpp"

#include "Hardware/Buzzer.hpp"
#include "Hardware/Mem.hpp"
#include "Hardware/Reset.hpp"
#include "Hardware/SerialIo.hpp"
#include "Hardware/SysTick.hpp"

#include "Icons/Icons.hpp"
#include "Library/Misc.hpp"
#include "ObjectModel/BedOrChamber.hpp"
#include "ObjectModel/PrinterStatus.hpp"
#include "PanelDue.hpp"
#include "Version.hpp"

#if DISPLAY_X == 800
extern const uint8_t glcd19x21[];
#endif

#include <General/SafeStrtod.h>
#include <General/SafeVsnprintf.h>
#include <General/SimpleMath.h>
#include <General/String.h>
#include <General/StringFunctions.h>

#include <ObjectModel/Axis.hpp>
#include <ObjectModel/Utils.hpp>

#include <UI/MessageLog.hpp>
#include <UI/Popup.hpp>
#include <UI/UserInterfaceConstants.hpp>

MainWindow mgr;

#define DEBUG 0
#include "Debug.hpp"

// Public fields
TextField *fwVersionField, *userCommandField, *ipAddressField;
IntegerField *freeMem;
StaticTextField *touchCalibInstruction, *debugField;
StaticTextField *messageTextFields[numMessageRows], *messageTimeFields[numMessageRows];

static const ColourScheme *colours;

// Private fields
static const size_t machineNameLength = 30;
static const size_t printingFileLength = 40;
static const size_t zprobeBufLength = 12;
static const size_t generatedByTextLength = 50;
static const size_t lastModifiedTextLength = 20;
static const size_t printTimeTextLength = 12;		// e.g. 11h 55m
static const size_t controlPageMacroTextLength = 50;
static const size_t ipAddressLength = 45;	// IPv4 needs max 15 but IPv6 can go up to 45

static String<ipAddressLength> ipAddress;

struct FileListButtons
{
	SingleButton *scrollLeftButton, *scrollRightButton, *folderUpButton;
	IntegerField *errorField;
};

static StaticTextField *fileListPopupNoFiles;

static FileListButtons filesListButtons, macrosListButtons;
static SingleButton *changeCardButton;

static TextButton *filenameButtons[NumDisplayedFiles];
static TextButton *macroButtons[NumDisplayedMacros];
static TextButton *controlPageMacroButtons[NumControlPageMacroButtons];
static String<controlPageMacroTextLength> controlPageMacroText[NumControlPageMacroButtons];

static PopupWindow *setTempPopup, *setRPMPopup, *movePopup, *extrudePopup, *fileListPopup, *macrosPopup, *fileDetailPopup, *baudPopup,
		*volumePopup, *infoTimeoutPopup, *screensaverTimeoutPopup, *babystepAmountPopup, *feedrateAmountPopup, *areYouSurePopup, *keyboardPopup, *coloursPopup, *screensaverPopup, *firmwareUpdatePopup;
static StaticTextField *areYouSureTextField, *areYouSureQueryField;
static DisplayField *emptyRoot, *baseRoot, *commonRoot, *controlRoot, *controlToolsRoot, *controlMovementRoot, *controlExtrusionRoot, *controlMacrosRoot, *printRoot, *statusJobStatusRoot, *statusTuneRoot, *statusJobRoot, *statusObjectsRoot, *messageRoot, *setupRoot;
static SingleButton *homeAllButton, *bedCompButton;
static IconButtonWithText *homeButtons[MaxDisplayableAxes], *toolButtons[MaxSlots];

// -- START: New fields for subpage grouping and top tabs (CONTROL/STATUS/SYSTEM)
// Top-subtab buttons for CONTROL page
static TextButton *controlTopTools = nullptr;
static TextButton *controlTopMovement = nullptr;
static TextButton *controlTopExtrusion = nullptr;
static TextButton *controlTopMacros = nullptr;

// Arrays to hold pointers to fields belonging to each subpage so we can show/hide them as a group
static DisplayField *controlToolsFields[64];
static size_t controlToolsFieldCount = 0;
static DisplayField *controlMovementFields[64];
static size_t controlMovementFieldCount = 0;
static DisplayField *controlExtrusionFields[64];
static size_t controlExtrusionFieldCount = 0;
static DisplayField *controlMacrosFields[64];
static size_t controlMacrosFieldCount = 0;

// STATUS subpages
static DisplayField *statusJobStatusFields[64];
static size_t statusJobStatusFieldCount = 0;
static DisplayField *statusTuneFields[64];
static size_t statusTuneFieldCount = 0;
static DisplayField *statusJobFields[64];
static size_t statusJobFieldCount = 0;

// SYSTEM subpages
static DisplayField *systemConsoleFields[64];
static size_t systemConsoleFieldCount = 0;
static DisplayField *systemSettingsFields[64];
static size_t systemSettingsFieldCount = 0;

static UiPage currentUiPage = UiPage::ControlTools;

#if DISPLAY_X == 800
// SYSTEM > SETTINGS modern 800x480 page and unified popups.
static ModernTextButton *settingsIpValueButton = nullptr;
static ModernTextButton *settingsFeedrateValueButton = nullptr;
static ModernTextButton *settingsBabystepValueButton = nullptr;
static ModernTextButton *settingsAlwaysDimButton = nullptr;
static ModernTextButton *settingsHeaterCombineButton = nullptr;
static String<48> settingsIpText;
static String<16> settingsFeedrateText;
static String<16> settingsBabystepText;

// Shared standard popup. VOLUME, BRIGHTNESS, INFO TIMEOUT, BAUD, ACCENT
// All SYSTEM > SETTINGS standard popups are migrated to the shared popup.
enum class StandardPopupContext : uint8_t
{
	None,
	SettingsVolume,
	SettingsBrightness,
	SettingsInfoTimeout,
	SettingsBaud,
	SettingsAccent,
	SettingsTouchCalibration,
	SettingsHeaterCombination,
	SettingsFactoryReset,
	TuneSpeed,
	TuneFan,
	TuneFlow,
	TunePressureAdvance,
	StatusObjectCancel,
	ControlMacroRun,
	JobStatusConfirm,
	ControlHeaterOff,
	ControlToolChange,
	StatusJobDelete,
	StatusJobStart,
	ModernAlert,
	ModernInfo,
	M291Confirm,
	M291ConfirmControls,
	M291Choices,
	M291NumberInt,
	M291NumberFloat,
	M291Text,
	SettingsFeedrate,
	SettingsBabystep
};
static StandardPopupContext standardPopupContext = StandardPopupContext::None;
static PopupWindow *standardPopup = nullptr;
static ModernCard *standardPopupNameCard = nullptr;
static StaticTextField *standardPopupNameField = nullptr;
static ModernCard *standardPopupInfoCard = nullptr;
static StaticTextField *standardPopupInfoFields[4] = { nullptr };
static ModernTextButton *standardPopupChoiceButtons[16] = { nullptr };
static ModernIconButton *standardPopupTrashButton = nullptr;
static ModernIconButton *standardPopupCancelButton = nullptr;
static ModernIconButton *standardPopupConfirmButton = nullptr;
static int standardPopupChoiceValues[16] = { 0 };
static size_t standardPopupChoiceCount = 0;
static uint32_t standardPopupM291Seq = 0;
static String<32> standardPopupM291ValueText;
// M291 choice labels are copied here because PanelDue.cpp calls currentAlert.Reset()
// (which clears alert.choices[]) at the start of every received message, and the
// choice buttons only hold a pointer to their text.
static String<16> standardPopupM291ChoiceText[10];
static int32_t standardPopupM291IntMin = 0;
static int32_t standardPopupM291IntMax = INT32_MAX;
static float standardPopupM291FloatMin = 0.0f;
static float standardPopupM291FloatMax = FLT_MAX;
static int32_t standardPopupM291TextMin = 0;
static int32_t standardPopupM291TextMax = 32;
static int standardPopupM291SelectedAxis = -1;
static char standardPopupM291SelectedAxisLetter = '\0';
static size_t standardPopupM291AxisButtonCount = 0;
static const char * const standardPopupM291JogText[6] = { LESS_ARROW "2.0", LESS_ARROW "0.2", LESS_ARROW "0.02", MORE_ARROW "0.02", MORE_ARROW "0.2", MORE_ARROW "2.0" };
static const char * const standardPopupM291JogParam[6] = { "-2.0", "-0.2", "-0.02", "0.02", "0.2", "2.0" };

// Shared-popup helpers are used by CONTROL/STATUS code that appears earlier
// in this translation unit than their implementations below.
static void ConfigureStandardPopupTitle(const char *title, bool longTitle);
static void ResetStandardPopupContent();
static void ConfigureStandardPopupInformation(const char *line1, const char *line2 = nullptr,
	const char *line3 = nullptr, const char *line4 = nullptr);
static void OpenM291TextKeyboard();
static void M291TextData(const char *data);

static int settingsPendingVolume = 0;
static int settingsPendingBrightness = 100;
static int settingsPendingInfoTimeout = DefaultInfoTimeout;
static int settingsPendingAccent = 1;
static int settingsPendingBaud = DefaultBaudRate;
static int settingsPendingFeedrate = 6000;
static int settingsPendingBabystepIndex = 0;

// CONTROL > TOOLS modern 800x480 page.
static constexpr unsigned int ControlToolVisibleColumns = 5;
static constexpr unsigned int ControlToolPagedColumns = 4;
static constexpr unsigned int ControlToolMaxHeaters = 32;
static unsigned int controlToolPage = 0;
static int controlToolActiveTarget[ControlToolMaxHeaters] = { 0 };
static int controlToolStandbyTarget[ControlToolMaxHeaters] = { 0 };

enum class ControlToolResourceType : uint8_t { None, Tool, Bed, Chamber };
struct ControlToolResource
{
	ControlToolResourceType type = ControlToolResourceType::None;
	int index = -1;
	int heater = -1;
};
static ControlToolResource controlToolVisibleResource[ControlToolVisibleColumns];
static ModernCard *controlToolHeaderCards[ControlToolVisibleColumns] = { nullptr };
static ModernTouchArea *controlToolHeaderButtons[ControlToolVisibleColumns] = { nullptr };	// invisible tap target over each header tile
static ModernResourceLabel *controlToolNameFields[ControlToolVisibleColumns] = { nullptr };
static StaticTextField *controlToolCurrentFields[ControlToolVisibleColumns] = { nullptr };
static ModernTemperatureButton *controlToolActiveButtons[ControlToolVisibleColumns] = { nullptr };
static ModernTemperatureButton *controlToolStandbyButtons[ControlToolVisibleColumns] = { nullptr };
static ModernPowerButton *controlToolPowerButtons[ControlToolVisibleColumns] = { nullptr };
static String<16> controlToolNameText[ControlToolVisibleColumns];
static String<20> controlToolCurrentText[ControlToolVisibleColumns];
static String<12> controlToolActiveText[ControlToolVisibleColumns];
static String<12> controlToolStandbyText[ControlToolVisibleColumns];
static ModernIconButton *controlToolPageUpButton = nullptr;
static ModernIconButton *controlToolPageDownButton = nullptr;
static constexpr uint32_t ControlToolsTemperatureRefreshInterval = 2000;
static uint32_t controlToolsLastTemperatureRefresh = 0;

enum class NumericPopupContext : uint8_t
{
	Temperature,
	M291Int,
	M291Float
};
static NumericPopupContext numericPopupContext = NumericPopupContext::Temperature;

static PopupWindow *controlTempNumpadPopup = nullptr;
static StaticTextField *controlTempNumpadValueField = nullptr;
static StaticTextField *controlTempNumpadUnitField = nullptr;
static ModernTextButton *controlTempNumpadResourceField = nullptr;
static ModernTextButton *controlTempNumpadDecimalButton = nullptr;
static ModernTextButton *controlTempNumpadMinusButton = nullptr;
static String<8> controlTempNumpadValueText;
static String<16> controlTempNumpadResourceText;
static ControlToolResource controlTempNumpadResource;
static bool controlTempNumpadActiveTarget = true;
static unsigned int controlTempNumpadValue = 0;
static bool controlTempNumpadFresh = true;
static String<32> numericPopupM291Text;
static bool numericPopupM291Fresh = true;

static String<12> controlToolChangeFromText;
static String<12> controlToolChangeToText;
static String<32> controlToolChangeSummaryText;
static int controlToolChangeTarget = NoTool;

static String<40> controlHeaterOffMessageText;
static ControlToolResource controlHeaterOffResource;

// CONTROL > MOVE modern 800x480 page.
static constexpr unsigned int ControlMoveAxisCount = 3;
static constexpr unsigned int ControlMoveStepCount = 5;
static const char * const controlMoveStepText[ControlMoveStepCount] = { "0.1", "0.02", "1", "10", "50" };
static unsigned int controlMoveSelectedStep = 2;       // 1 mm, as shown in the v7 mock-up
static StaticTextField *controlMovePositionFields[ControlMoveAxisCount] = { nullptr };
static String<16> controlMovePositionText[ControlMoveAxisCount];
static bool controlMovePositionValid[ControlMoveAxisCount] = { false, false, false };
static float controlMovePosition[ControlMoveAxisCount] = { 0.0f, 0.0f, 0.0f };
static ModernTextButton *controlMoveStepButtons[ControlMoveStepCount] = { nullptr };
static ModernHomeButton *controlMoveHomeButtons[ControlMoveAxisCount] = { nullptr };
static ModernHomeButton *controlMoveHomeAllButton = nullptr;
static ModernBedCompButton *controlMoveBedCompButton = nullptr;

// CONTROL > EXTRUDE modern 800x480 page. The tool rows are information-only;
// extrusion and retraction always use RRF's current active tool.
static constexpr unsigned int ControlExtrudeToolsPerPage = 4;
static constexpr unsigned int ControlExtrudeSpeedCount = 4;
static constexpr unsigned int ControlExtrudeDistanceCount = 4;
static const char * const controlExtrudeSpeedText[ControlExtrudeSpeedCount] = { "2", "5", "10", "20" };
static const unsigned int controlExtrudeSpeedFeedrate[ControlExtrudeSpeedCount] = { 120, 300, 600, 1200 };
static const char * const controlExtrudeDistanceText[ControlExtrudeDistanceCount] = { "10", "20", "50", "150" };
static const char * const controlExtrudeDistanceParam[ControlExtrudeDistanceCount] = { "10", "20", "50", "150" };
static unsigned int controlExtrudeToolPage = 0;
static unsigned int controlExtrudeSelectedSpeed = 1;       // 5 mm/s, as shown in v10
static unsigned int controlExtrudeSelectedDistance = 2;    // 50 mm, as shown in v10
static ModernCard *controlExtrudeActiveToolCard = nullptr;
static ModernResourceLabel *controlExtrudeActiveToolNameField = nullptr;
static StaticTextField *controlExtrudeActiveToolTempField = nullptr;
static String<12> controlExtrudeActiveToolNameText;
static String<20> controlExtrudeActiveToolTempText;
static ModernTextButton *controlExtrudeSpeedButtons[ControlExtrudeSpeedCount] = { nullptr };
static ModernTextButton *controlExtrudeDistanceButtons[ControlExtrudeDistanceCount] = { nullptr };
static ModernTextButton *controlExtrudeRetractButton = nullptr;
static ModernTextButton *controlExtrudeExtrudeButton = nullptr;
static ModernIconButton *controlExtrudePageUpButton = nullptr;
static ModernIconButton *controlExtrudePageDownButton = nullptr;
static float controlColdExtrudeTemperature = 0.0f;
static float controlColdRetractTemperature = 0.0f;
static bool controlColdExtrudeTemperatureValid = false;
static bool controlColdRetractTemperatureValid = false;

// CONTROL > MACROS modern 800x480 page. The embedded list has its own
// FileManager viewport so it does not disturb the legacy macros popup.
static constexpr unsigned int ControlMacroRows = 6;
static ModernTextButton *controlMacroFileButtons[ControlMacroRows] = { nullptr };
static ModernIconButton *controlMacroPageUpButton = nullptr;
static ModernIconButton *controlMacroPageDownButton = nullptr;
static bool controlMacroCanScrollEarlier = false;
static bool controlMacroCanScrollLater = false;
static bool controlMacroInSubdir = false;
static FileManager::Path controlMacroPendingFile;

// Reusable modern alert popup.  MOVE is the first consumer, but it is kept
// global so other modern pages can use the same interaction later.
static String<64> modernAlertMessageText;

// Modern presentation for ordinary RRF responses and simple informational
// notices. Rich M291 dialogs continue to use AlertPopup because they also
// provide choices, numeric/text entry and acknowledgement semantics.
static String<(alertTextLength + 3)/4> modernInfoText[4];
static bool displayingModernInfoPopup = false;

static constexpr unsigned int StatusObjectsPerPage = 5;
static constexpr unsigned int StatusMaxObjects = 20;       // RRF 3.6 exposes up to 20 build objects on Duet 2
static unsigned int statusObjectPage = 0;
static int selectedStatusObject = -1;
static int currentStatusObject = -1;
static int pendingStatusObjectCancel = -1;
static unsigned int statusObjectCount = 0;
static bool statusObjectsDirty = false;
static bool statusObjectsNeedFullRefresh = false;

struct StatusObjectInfo
{
	String<32> name;
	float xMin = 0.0f, xMax = 0.0f, yMin = 0.0f, yMax = 0.0f;
	bool present = false;
	bool cancelled = false;
	bool xValid = false;
	bool yValid = false;
};

static StatusObjectInfo statusObjects[StatusMaxObjects];
static ModernTextButton *statusObjectNumberButtons[StatusObjectsPerPage] = { nullptr };
static ModernTextButton *statusObjectNameButtons[StatusObjectsPerPage] = { nullptr };
static ModernIconButton *statusObjectPageUpButton = nullptr;
static ModernIconButton *statusObjectPageDownButton = nullptr;
static ModernTextButton *statusObjectMarkers[StatusMaxObjects] = { nullptr };
static String<4> statusObjectRowNumberText[StatusObjectsPerPage];
static String<32> statusObjectRowNameText[StatusObjectsPerPage];
static String<4> statusObjectMarkerText[StatusMaxObjects];
static String<40> statusObjectCancelNameText;

static float statusObjectAxisMin[MaxTotalAxes] = { 0.0f };
static float statusObjectAxisMax[MaxTotalAxes] = { 0.0f };
static bool statusObjectAxisMinValid[MaxTotalAxes] = { false };
static bool statusObjectAxisMaxValid[MaxTotalAxes] = { false };
static int statusObjectXAxis = -1, statusObjectYAxis = -1;

class StatusObjectMapField : public DisplayField
{
private:
	PixelNumber height;
	PixelNumber canvasX, canvasY, canvasWidth, canvasHeight;
	Colour pageColour, tileColour, mapBorderColour, axisColour;
	float xMin = 0.0f, xMax = 0.0f, yMin = 0.0f, yMax = 0.0f;
	bool boundsValid = false;

	void GetBedRect(PixelNumber& bx, PixelNumber& by, PixelNumber& bw, PixelNumber& bh) const
	{
		bx = canvasX; by = canvasY; bw = canvasWidth; bh = canvasHeight;
		if (!boundsValid)
		{
			return;
		}
		const float xs = xMax - xMin;
		const float ys = yMax - yMin;
		if (xs <= 0.0f || ys <= 0.0f)
		{
			return;
		}
		if (xs * static_cast<float>(canvasHeight) >= ys * static_cast<float>(canvasWidth))
		{
			bh = static_cast<PixelNumber>((static_cast<float>(canvasWidth) * ys / xs) + 0.5f);
			if (bh < 1) bh = 1;
			by = canvasY + (canvasHeight - bh) / 2;
		}
		else
		{
			bw = static_cast<PixelNumber>((static_cast<float>(canvasHeight) * xs / ys) + 0.5f);
			if (bw < 1) bw = 1;
			bx = canvasX + (canvasWidth - bw) / 2;
		}
	}

protected:
	PixelNumber GetHeight() const override { return height; }

public:
	StatusObjectMapField(PixelNumber py, PixelNumber px, PixelNumber pw, PixelNumber ph,
		PixelNumber pcx, PixelNumber pcy, PixelNumber pcw, PixelNumber pch,
		Colour page, Colour tile, Colour borderColour, Colour axes)
		: DisplayField(py, px, pw), height(ph), canvasX(pcx), canvasY(pcy),
		  canvasWidth(pcw), canvasHeight(pch), pageColour(page), tileColour(tile),
		  mapBorderColour(borderColour), axisColour(axes) { }

	void SetBounds(float pxMin, float pxMax, float pyMin, float pyMax, bool valid)
	{
		if (boundsValid != valid || xMin != pxMin || xMax != pxMax || yMin != pyMin || yMax != pyMax)
		{
			boundsValid = valid; xMin = pxMin; xMax = pxMax; yMin = pyMin; yMax = pyMax;
			changed = true;
		}
	}

	void GetBedBounds(PixelNumber& bx, PixelNumber& by, PixelNumber& bw, PixelNumber& bh) const
	{
		GetBedRect(bx, by, bw, bh);
	}

	bool Project(float px, float py, PixelNumber& screenX, PixelNumber& screenY) const
	{
		if (!boundsValid || xMax <= xMin || yMax <= yMin)
		{
			return false;
		}
		PixelNumber bx, by, bw, bh;
		GetBedRect(bx, by, bw, bh);
		float xr = (px - xMin) / (xMax - xMin);
		float yr = (py - yMin) / (yMax - yMin);
		if (xr < 0.0f) xr = 0.0f; else if (xr > 1.0f) xr = 1.0f;
		if (yr < 0.0f) yr = 0.0f; else if (yr > 1.0f) yr = 1.0f;
		screenX = bx + static_cast<PixelNumber>(xr * static_cast<float>(bw - 1));
		screenY = by + bh - 1 - static_cast<PixelNumber>(yr * static_cast<float>(bh - 1));
		return true;
	}

	void Refresh(bool full, PixelNumber xOffset, PixelNumber yOffset) override
	{
		if (!full && !changed) return;
		lcd.setColor(pageColour);
		lcd.fillRect(x + xOffset, y + yOffset, x + xOffset + width - 1, y + yOffset + height - 1);

		PixelNumber bx, by, bw, bh;
		GetBedRect(bx, by, bw, bh);
		bx += xOffset; by += yOffset;
		lcd.setColor(tileColour);
		lcd.fillRect(bx, by, bx + bw - 1, by + bh - 1);
		lcd.setColor(mapBorderColour);
		lcd.drawRect(bx, by, bx + bw - 1, by + bh - 1);
		if (bw > 2 && bh > 2) lcd.drawRect(bx + 1, by + 1, bx + bw - 2, by + bh - 2);

		const int ox = static_cast<int>(bx) - 12;
		const int oy = static_cast<int>(by + bh) + 12;
		const int xt = static_cast<int>(bx + bw) + 12;
		const int yt = static_cast<int>(by) - 6;
		lcd.setColor(axisColour);
		lcd.drawLine(ox, oy, xt, oy);
		lcd.drawLine(ox, oy, ox, yt);
		lcd.drawLine(xt, oy, xt - 8, oy - 5);
		lcd.drawLine(xt, oy, xt - 8, oy + 5);
		lcd.drawLine(ox, yt, ox - 5, yt + 8);
		lcd.drawLine(ox, yt, ox + 5, yt + 8);
		lcd.setTransparentBackground(true);
		lcd.setFont(glcd19x21);
		lcd.setTextPos((ox > 18) ? ox - 18 : 0, oy + 4); lcd.printf("0");
		lcd.setTextPos(xt - 2, oy + 4); lcd.printf("X");
		lcd.setTextPos((ox > 7) ? ox - 7 : 0, (yt > 24) ? yt - 24 : 0); lcd.printf("Y");
		lcd.setTransparentBackground(false);
		changed = false;
	}
};

static StatusObjectMapField *statusObjectMap = nullptr;

static constexpr unsigned int StatusJobRows = 6;
static ModernTextButton *statusJobFileButtons[StatusJobRows] = { nullptr };
static ModernTextButton *statusJobSdButton = nullptr;
static ModernIconButton *statusJobPageUpButton = nullptr;
static ModernIconButton *statusJobPageDownButton = nullptr;
static unsigned int statusJobNumVolumes = 1;
static bool statusJobCanScrollEarlier = false;
static bool statusJobCanScrollLater = false;
static bool statusJobInSubdir = false;

// STATUS > PRINTING. The nine cards are intentionally read-only; tuning belongs on STATUS > TUNE.
enum class JobStatusTileType : uint8_t
{
	ToolTemp, BedTemp, ChamberTemp, FanPart, FanAux, FanCha, SpeedReq, SpeedCur, FlowFactor, FlowVol
};
static constexpr unsigned int JobStatusTileCount = 9;
static constexpr unsigned int JobStatusMaxHeaters = 32;
static JobStatusTileType jobStatusTiles[JobStatusTileCount] =
{
	JobStatusTileType::ToolTemp, JobStatusTileType::BedTemp, JobStatusTileType::ChamberTemp,
	JobStatusTileType::FanPart, JobStatusTileType::FanAux, JobStatusTileType::FanCha,
	JobStatusTileType::SpeedCur, JobStatusTileType::FlowFactor, JobStatusTileType::FlowVol
};
static ModernCard *jobStatusCards[JobStatusTileCount] = { nullptr };
static StaticTextField *jobStatusLabels[JobStatusTileCount] = { nullptr };
static StaticTextField *jobStatusValues[JobStatusTileCount] = { nullptr };
static String<20> jobStatusLabelText[JobStatusTileCount];
static String<24> jobStatusValueText[JobStatusTileCount];
static ModernCard *jobStatusNameCard = nullptr, *jobStatusProgressCard = nullptr, *jobStatusThumbnailCard = nullptr;
static StaticTextField *jobStatusNameField = nullptr, *jobStatusProgressField = nullptr;
static StaticTextField *jobStatusLayersField = nullptr, *jobStatusTimeField = nullptr;
static String<72> jobStatusNameText;
static String<16> jobStatusProgressText, jobStatusLayersText, jobStatusTimeText;
static ModernTextButton *jobStatusPauseResumeButton = nullptr, *jobStatusAbortButton = nullptr;
static DrawDirect *jobStatusThumbnail = nullptr;
static String<32> jobStatusConfirmText;
enum class JobStatusConfirmAction : uint8_t { None, Pause, Resume, Abort };
static JobStatusConfirmAction jobStatusConfirmAction = JobStatusConfirmAction::None;
static float jobStatusHeaterTemps[JobStatusMaxHeaters] = { 0.0f };
static bool jobStatusHeaterValid[JobStatusMaxHeaters] = { false };
static OM::HeaterStatus jobStatusHeaterStatus[JobStatusMaxHeaters] = { OM::HeaterStatus::off };
static constexpr unsigned int JobStatusMaxExtruders = 8;
static float jobStatusFilamentDiameter[JobStatusMaxExtruders] = { 0.0f };
static bool jobStatusFilamentDiameterValid[JobStatusMaxExtruders] = { false };
static float jobStatusRequestedSpeed = 0.0f, jobStatusTopSpeed = 0.0f, jobStatusExtrusionRate = 0.0f;
// VOL. FLOW is shown as a running average of the extrusion rate with a time constant of about 3 s, because the
// instantaneous rate jumps around (travel moves, retractions, acceleration).
static float jobStatusExtrusionRateAvg = 0.0f;
static uint32_t jobStatusExtrusionRateTime = 0;
static bool jobStatusExtrusionRateHaveAvg = false;
static constexpr uint32_t JobStatusFlowAverageMs = 3000;
static constexpr uint32_t JobStatusFlowStaleMs = 10000;
static unsigned int jobStatusLayer = 0, jobStatusNumLayers = 0, jobStatusProgress = 0;
static uint32_t jobStatusDuration = 0;
static uint32_t jobStatusLastLiveRefresh = 0;

static constexpr unsigned int TuneToolsPerPage = 4;
static PixelNumber tuneRowLabelX[3] = { 0, 0, 0 }, tuneRowTileX[3] = { 0, 0, 0 };
static constexpr PixelNumber tuneRowTileW = 110;
static constexpr unsigned int TuneMaxExtruders = 8;
static constexpr unsigned int TuneMaxFans = 16;
static unsigned int tuneToolPage = 0;

static ModernTextButton *tuneSpeedButton = nullptr;
static ModernTextButton *tuneGeneralFanButtons[2] = { nullptr, nullptr };
static StaticTextField *tuneGeneralFanLabels[2] = { nullptr, nullptr };
static int8_t tuneGeneralFanIndices[2] = { -1, -1 };
static ModernTextButton *tuneToolNumberButtons[TuneToolsPerPage] = { nullptr };
static ModernTextButton *tuneToolFanButtons[TuneToolsPerPage] = { nullptr };
static ModernTextButton *tuneToolFlowButtons[TuneToolsPerPage] = { nullptr };
static ModernTextButton *tuneToolPaButtons[TuneToolsPerPage] = { nullptr };
static ModernTextButton *tuneZOffsetButton = nullptr;
static ModernTextButton *tuneZPlusButton = nullptr, *tuneZMinusButton = nullptr;
static ModernIconButton *tunePageUpButton = nullptr, *tunePageDownButton = nullptr;

static String<8> tuneToolNumberText[TuneToolsPerPage];
static String<12> tuneToolFanText[TuneToolsPerPage];
static String<12> tuneToolFlowText[TuneToolsPerPage];
static String<16> tuneToolPaText[TuneToolsPerPage];
static String<12> tuneSpeedText;
static String<12> tuneGeneralFanText[2];
static String<16> tuneZOffsetText;
static String<12> tuneZPlusText, tuneZMinusText;

static int tuneExtruderFactor[TuneMaxExtruders] = { 100, 100, 100, 100, 100, 100, 100, 100 };
static float tunePressureAdvance[TuneMaxExtruders] = { 0.0f };
static bool tunePressureAdvanceValid[TuneMaxExtruders] = { false };
static int tuneFanPercent[TuneMaxFans] = { 0 };
static bool tuneFanValid[TuneMaxFans] = { false };
static bool tuneFanThermostatic[TuneMaxFans] = { false };		// fans with thermostatic control (e.g. a heat-break fan)
static String<16> tuneFanNames[TuneMaxFans];
static int tuneSpeedPercent = 100;

static String<32> tunePopupTitleText;
static String<16> tunePopupValueText;

enum class TunePopupKind : uint8_t { None, Speed, Fan, Flow, PressureAdvance };
static TunePopupKind tunePopupKind = TunePopupKind::None;
static int tunePopupResource = -1;
static int tunePopupPercent = 0;
static float tunePopupPa = 0.0f;
#else
static constexpr unsigned int StatusObjectsPerPage = 6;
static unsigned int statusObjectPage = 0;
static unsigned int selectedStatusObject = 0;
#endif

// Register a field as belonging to a UI page.
// We keep this temporarily while migrating away from the old
// per-page field arrays.
static void RegisterField(DisplayField *arr[], size_t &count, DisplayField *f)
{
	if (f != nullptr && count < 64)
	{
		arr[count++] = f;
	}
}

// Legacy group show/hide helper.
// Keep this for now while the new UiPage system is being introduced.
static void ShowGroup(DisplayField *arr[], size_t count, bool show)
{
	for (size_t i = 0; i < count; ++i)
	{
		mgr.Show(arr[i], show);
	}
}

// Show one of the new UI pages.
//
// Fields with UiPage::None are left alone so the existing PanelDue
// interface continues to work while we migrate it to the new system.
static void ShowUiPage(UiPage page)
{
	currentUiPage = page;

	for (DisplayField *field = mgr.GetRoot(); field != nullptr; field = field->next)
	{
		const UiPage fieldPage = field->GetUiPage();

		if (fieldPage != UiPage::None)
		{
			field->Show(fieldPage == page);
		}
	}

	mgr.Refresh(true);
}

// -- END: UI page handling

static float axisMaxVal = 0.0;
static FloatField *controlTabAxisPos[MaxDisplayableAxes];
#if DISPLAY_X == 800
static FloatField *printTabAxisPos[MaxDisplayableAxes];
#endif
static FloatField *movePopupAxisPos[MaxDisplayableAxes];
static FloatField *currentTemps[MaxSlots];
static FloatField *fpHeightField, *fpLayerHeightField, *babystepOffsetField;
static TextButtonWithLabel *babystepMinusButton, *babystepPlusButton;
static IntegerField *fpSizeField, *fpFilamentField, *filePopupTitleField;
static ProgressBar *printProgressBar;
static SingleButton *railStopButton, *tabControl, *tabStatus, *tabSystem, *railAlertButton;
#if DISPLAY_X == 800
static ModernCard *railBackground = nullptr, *topTabSeparator = nullptr;
#endif
static ButtonBase *filesButton, *pauseButton, *resumeButton, *cancelButton, *babystepButton, *reprintButton;
static TextField *timeLeftField, *zProbe;
static TextField *fpNameField, *fpGeneratedByField, *fpLastModifiedField, *fpPrintTimeField;
DrawDirect *fpThumbnail;
static StaticTextField *moveAxisRows[MaxDisplayableAxes];
static StaticTextField *nameField, *statusField;
static StaticTextField *screensaverText;
static IntegerButton *activeTemps[MaxSlots], *standbyTemps[MaxSlots];
static IntegerButton *spd, *extrusionFactors[MaxSlots], *fanSpeed, *baudRateButton, *volumeButton, *infoTimeoutButton, *screensaverTimeoutButton, *feedrateAmountButton;
static TextButton *coloursButton, *dimmingTypeButton, *heaterCombiningButton, *logLevelButton;
static TextButtonWithLabel *babystepAmountButton;
static SingleButton *moveButton, *extrudeButton, *macroButton;
static PopupWindow *babystepPopup;
static AlertPopup *alertPopup;
static CharButtonRow *keyboardRows[4];
static const char* _ecv_array const * _ecv_array currentKeyboard;
static void (*keyboardDataHandler)(const char *data) = nullptr;

constexpr PixelNumber masterTabWidth = (DISPLAY_X == 480) ? 84 : 90;
constexpr PixelNumber contentLeft = masterTabWidth + margin;
constexpr PixelNumber contentWidth = DisplayX - contentLeft - margin;
// Top sub-tab row height for the 800x480 modern UI (taller than the shared
// buttonHeight used by many other rows, per the mock-up). contentTop tracks
// it directly so page content still starts right below the tab row.
constexpr PixelNumber topTabHeight = (DISPLAY_X == 800) ? 56 : buttonHeight;
constexpr PixelNumber contentTop = topTabHeight;
// The top tab row itself extends further left than the page content below
// it, stopping only 20px short of the STOP/rail icon column (matching the
// 20px gap already used between the CONTROL/STATUS/SYSTEM rail tiles), so
// it visually reaches toward STOP rather than starting at contentLeft.
constexpr PixelNumber topTabRowLeft = (DISPLAY_X == 800) ? 103 : contentLeft;
constexpr PixelNumber topTabRowWidth = (DISPLAY_X == 800) ? (DisplayX - margin - topTabRowLeft) : contentWidth;

static bool IsPermanentRailField(const DisplayField *field)
{
#if DISPLAY_X == 800
	// The whole five-button rail is one permanent UI group.  None of these
	// fields (or the rail backing card / tab separator) may be pushed into the
	// content pane by RelayoutLegacyFields().
	return field == railStopButton || field == tabControl || field == tabStatus ||
		field == tabSystem || field == railAlertButton || field == railBackground ||
		field == topTabSeparator;
#else
	return field == tabControl || field == tabStatus || field == tabSystem;
#endif
}

// Move full-screen legacy fields into the content pane to the right of the master rail.
static void RelayoutLegacyFields()
{
	DisplayField *seen[512];
	size_t seenCount = 0;
#if DISPLAY_X == 800
	// STATUS > OBJECT and SYSTEM > SETTINGS are native 800x480 modern pages and
	// are already positioned in the content pane. Do not relayout them.
	DisplayField * const roots[] = { controlRoot, printRoot, messageRoot };
#else
	DisplayField * const roots[] = { controlRoot, printRoot, statusObjectsRoot, messageRoot, setupRoot };
#endif

	for (DisplayField *root : roots)
	{
		for (DisplayField *field = root; field != nullptr; field = field->next)
		{
			bool alreadySeen = false;
			for (size_t i = 0; i < seenCount; ++i)
			{
				if (seen[i] == field)
				{
					alreadySeen = true;
					break;
				}
			}
			if (alreadySeen)
			{
				continue;
			}

			_ecv_assert(seenCount < ARRAY_SIZE(seen));
			seen[seenCount++] = field;
			if (IsPermanentRailField(field))
			{
				continue;
			}

			const PixelNumber oldX = field->GetMinX();
			const PixelNumber oldWidth = field->GetMaxX() - oldX + 1;
			const PixelNumber newX = contentLeft + static_cast<PixelNumber>((static_cast<uint32_t>(oldX) * contentWidth) / DisplayX);
			PixelNumber newWidth = static_cast<PixelNumber>((static_cast<uint32_t>(oldWidth) * contentWidth) / DisplayX);
			if (newWidth == 0)
			{
				newWidth = 1;
			}

			field->SetPositionAndWidth(newX, newWidth);
			field->SetPosition(newX, field->GetMinY() + contentTop);
		}
	}
}

static Colour GetModernAccentColourByIndex(uint8_t index)
{
	switch (index % 8)
	{
	case 0: return UTFT::fromRGB(238, 112, 4);   // 1 Orange Fire  #EE7004
	case 1: return UTFT::fromRGB(255, 0, 0);     // 2 Red Voron
	case 2: return UTFT::fromRGB(0, 124, 247);   // 3 Duet Blue
	case 3: return UTFT::fromRGB(0, 255, 255);   // 4 Cyan
	case 4: return UTFT::fromRGB(80, 150, 56);   // 5 RepRap Green  #509638
	case 5: return UTFT::fromRGB(252, 209, 10);  // 6 Yellow / Gold
	case 6: return UTFT::fromRGB(103, 255, 0);   // 7 Lime
	default:return UTFT::fromRGB(255, 0, 255);   // 8 Magenta
	}
}

static Colour GetModernAccentColour()
{
	return GetModernAccentColourByIndex(nvData.GetAccentColour());
}

static ModernTextButton *AddTopTab(unsigned int index, unsigned int count, const char *label, Event event)
{
	const PixelNumber width = topTabRowWidth / count;
	const Colour tile = UTFT::fromRGB(28, 34, 43);        // #1c222b
	const Colour text = UTFT::fromRGB(229, 232, 236);     // #e5e8ec
	const Colour accent = GetModernAccentColour();     // #e2453f

	DisplayField::SetDefaultColours(text, tile, accent, tile, accent, accent, IconPaletteDark);
	// No per-tab border: the mock-up distinguishes the active tab purely by
	// its Accent fill (handled in ModernTextButton::Refresh via `pressed`),
	// with a single shared separator line under the whole row instead of an
	// outline around every tab.
	ModernTextButton * const tab = new ModernTextButton(0, topTabRowLeft + index * width, width, topTabHeight,
		label, event, 0, DEFAULT_FONT, false);
	mgr.AddField(tab);
	return tab;
}

// Fill the complete top-tab strip behind the rounded tab buttons. Add this
// after the tabs because DisplayFieldManager::AddField() prepends: the strip
// then renders first and prevents the page background showing through at the
// rounded tab corners.
static void AddTopTabBackground()
{
	const Colour tile = UTFT::fromRGB(28, 34, 43);            // #1c222b
	mgr.AddField(new ModernCard(0, topTabRowLeft, topTabRowWidth, topTabHeight, tile, tile, false));
}

#if DISPLAY_X == 800
// Top (sub) tabs. Every modern page owns its own set of tab buttons, so their "selected" look cannot be left to the
// touch/release handling: a button pressed on one page stays pressed there and shows up highlighted (wrongly) when that
// page is displayed again, while the tab of the page you just switched to was never pressed at all.
// Instead, work out which tab belongs to the page being displayed and press exactly that one.
static bool IsTopTabEvent(Event e)
{
	return e == evControlTools || e == evControlMovement || e == evControlExtrusion || e == evControlMacros
		|| e == evStatusJobStatus || e == evStatusTune || e == evStatusJob || e == evStatusObjects
		|| e == evSystemConsole || e == evSystemSettings;
}

static Event GetTopTabEventForPage(UiPage page)
{
	switch (page)
	{
	case UiPage::ControlTools:		return evControlTools;
	case UiPage::ControlMovement:	return evControlMovement;
	case UiPage::ControlExtrusion:	return evControlExtrusion;
	case UiPage::ControlMacros:		return evControlMacros;
	case UiPage::StatusJobStatus:	return evStatusJobStatus;
	case UiPage::StatusTune:		return evStatusTune;
	case UiPage::StatusJob:			return evStatusJob;
	case UiPage::StatusObjects:		return evStatusObjects;
	case UiPage::SystemConsole:		return evSystemConsole;
	case UiPage::SystemSettings:	return evSystemSettings;
	default:						return evNull;
	}
}

// Highlight the top tab of the current page and release all the other top tabs in the displayed root.
static void SyncTopTabHighlight()
{
	const Event selected = GetTopTabEventForPage(currentUiPage);
	if (selected == evNull)
	{
		return;
	}
	for (DisplayField *f = mgr.GetRoot(); f != nullptr; f = f->next)
	{
		if (f->IsButton())
		{
			ButtonBase * const b = static_cast<ButtonBase*>(f);
			const Event e = static_cast<Event>(b->GetEvent());
			if (IsTopTabEvent(e))
			{
				b->Press(e == selected, 0);
			}
		}
	}
}
#endif

static ButtonBase * null currentTab = nullptr;

static ButtonPress currentButton;
static ButtonPress fieldBeingAdjusted;
static ButtonPress currentExtrudeRatePress, currentExtrudeAmountPress;

static String<machineNameLength> machineName;
static String<printingFileLength> printingFile;
static bool lastJobFileNameAvailable = false;
static String<zprobeBufLength> zprobeBuf;
static String<generatedByTextLength> generatedByText;
static String<lastModifiedTextLength> lastModifiedText;
static String<printTimeTextLength> printTimeText;

const size_t maxUserCommandLength = 40;					// max length of a user gcode command
const size_t numUserCommandBuffers = 6;					// number of command history buffers plus one

static String<maxUserCommandLength> userCommandBuffers[numUserCommandBuffers];
static size_t currentUserCommandBuffer = 0, currentHistoryBuffer = 0;

static unsigned int numToolColsUsed = 0;
static unsigned int numHeaterAndToolColumns = 0;
static int oldIntValue;
static Event eventToConfirm = evNull;
static uint8_t numVisibleAxes = 0;						// initialise to 0 so we refresh the macros list when we receive the number of axes
static uint8_t numDisplayedAxes = 0;
static bool isDelta = false;

const char* _ecv_array null currentFile = nullptr;			// file whose info is displayed in the file info popup
const StringTable * strings = &LanguageTables[0];
static bool keyboardIsDisplayed = false;
static bool keyboardShifted = false;

int32_t alertMode = -1;									// the mode of the current alert, or -1 if no alert displayed
uint32_t alertTicks = 0;
uint32_t infoTimeout = DefaultInfoTimeout;				// info timeout in seconds, 0 means don't display into messages at all
uint32_t whenAlertReceived;
bool displayingResponse = false;						// true if displaying a response

static PixelNumber screensaverTextWidth = 0;
static uint32_t lastScreensaverMoved = 0;

static uint8_t currentWorkplaceNumber = OM::MaxTotalWorkplaces;
static int8_t currentTool = -2;							// Initialized to a value never returned by RRF to have the logic for "no tool" applied at startup
static bool allAxesHomed = false;
static const bool isLandscape = true; 					// Once portrait mode is enabled, this needs to be de-const-ed

#ifdef SUPPORT_ENCODER

# include "Hardware/RotaryEncoder.hpp"

static RotaryEncoder *encoder;
static uint32_t lastEncoderCommandSentAt = 0;
#endif

inline PixelNumber CalcWidth(unsigned int numCols, PixelNumber displayWidth = DisplayX)
{
	return (displayWidth - 2 * margin + fieldSpacing)/numCols - fieldSpacing;
}

inline PixelNumber CalcXPos(unsigned int col, PixelNumber width, int offset = 0)
{
	return col * (width + fieldSpacing) + margin + offset;
}

// Add a text button with a string parameter
TextButton *AddTextButton(PixelNumber row, unsigned int col, unsigned int numCols, const char* _ecv_array text, Event evt, const char* param, PixelNumber displayWidth = DisplayX)
{
	PixelNumber width = CalcWidth(numCols, displayWidth);
	PixelNumber xpos = CalcXPos(col, width);
	TextButton *f = new TextButton(row - 2, xpos, width, text, evt, param);
	mgr.AddField(f);
	return f;
}

// Add a text button with an int parameter
TextButton *AddTextButton(PixelNumber row, unsigned int col, unsigned int numCols, const char* _ecv_array text, Event evt, int param, PixelNumber displayWidth = DisplayX)
{
	PixelNumber width = CalcWidth(numCols, displayWidth);
	PixelNumber xpos = CalcXPos(col, width);
	TextButton *f = new TextButton(row - 2, xpos, width, text, evt, param);
	mgr.AddField(f);
	return f;
}

// Add an integer button
IntegerButton *AddIntegerButton(PixelNumber row, unsigned int col, unsigned int numCols, const char * _ecv_array null label, const char * _ecv_array null units, Event evt, PixelNumber displayWidth = DisplayX)
{
	PixelNumber width = CalcWidth(numCols, displayWidth);
	PixelNumber xpos = CalcXPos(col, width);
	IntegerButton *f = new IntegerButton(row - 2, xpos, width, label, units);
	f->SetEvent(evt, 0);
	mgr.AddField(f);
	return f;
}

// Add an icon button with a string parameter
IconButton *AddIconButton(PixelNumber row, unsigned int col, unsigned int numCols, Icon icon, Event evt, const char* param, PixelNumber displayWidth = DisplayX)
{
	PixelNumber width = CalcWidth(numCols, displayWidth);
	PixelNumber xpos = CalcXPos(col, width);
	IconButton *f = new IconButton(row - 2, xpos, width, icon, evt, param);
	mgr.AddField(f);
	return f;
}

// Add an icon button with an int parameter
IconButton *AddIconButton(PixelNumber row, unsigned int col, unsigned int numCols, Icon icon, Event evt, int param, PixelNumber displayWidth = DisplayX)
{
	PixelNumber width = CalcWidth(numCols, displayWidth);
	PixelNumber xpos = CalcXPos(col, width);
	IconButton *f = new IconButton(row - 2, xpos, width, icon, evt, param);
	mgr.AddField(f);
	return f;
}

// Add an icon button with a string parameter
IconButtonWithText *AddIconButtonWithText(PixelNumber row, unsigned int col, unsigned int numCols, Icon icon, Event evt, const char * text, const char* param, PixelNumber displayWidth = DisplayX)
{
	PixelNumber width = CalcWidth(numCols, displayWidth);
	PixelNumber xpos = CalcXPos(col, width);
	IconButtonWithText *f = new IconButtonWithText(row - 2, xpos, width, icon, evt, text, param);
	mgr.AddField(f);
	return f;
}

// Add an icon button with an int parameter
IconButtonWithText *AddIconButtonWithText(PixelNumber row, unsigned int col, unsigned int numCols, Icon icon, Event evt, int intVal, const int param, PixelNumber displayWidth = DisplayX)
{
	PixelNumber width = CalcWidth(numCols, displayWidth);
	PixelNumber xpos = CalcXPos(col, width);
	IconButtonWithText *f = new IconButtonWithText(row - 2, xpos, width, icon, evt, intVal, param);
	mgr.AddField(f);
	return f;
}

// Add an icon button with an int parameter
IconButtonWithText *AddIconButtonWithText(PixelNumber row, unsigned int col, unsigned int numCols, Icon icon, Event evt, const char * text, const int param, PixelNumber displayWidth = DisplayX)
{
	PixelNumber width = CalcWidth(numCols, displayWidth);
	PixelNumber xpos = CalcXPos(col, width);
	IconButtonWithText *f = new IconButtonWithText(row - 2, xpos, width, icon, evt, text, param);
	mgr.AddField(f);
	return f;
}

// Create a row of text buttons.
// Optionally, set one to 'pressed' and return that one.
// Set the colours before calling this
ButtonPress CreateStringButtonRow(
		Window * parentWindow,
		PixelNumber top,
		PixelNumber left,
		PixelNumber totalWidth,
		PixelNumber spacing,
		unsigned int numButtons,
		const char* _ecv_array const text[],
		const char* _ecv_array const params[],
		Event evt,
		int selected = -1,
		bool textButtonForAxis = false,
		DisplayField** firstButton = nullptr)
{
	const PixelNumber step = (totalWidth + spacing)/numButtons;
	ButtonPress bp;
	// Since Window->AddField prepends fields in the linked list we start with the last element
	for (int i = numButtons - 1; i >= 0; --i)
	{
		TextButton *tp =
				textButtonForAxis
				? new TextButtonForAxis(top, left + i * step, step - spacing, text[i], evt, params[i])
				: new TextButton(		top, left + i * step, step - spacing, text[i], evt, params[i]);
		parentWindow->AddField(tp);
		if ((int)i == selected)
		{
			tp->Press(true, 0);
			bp = ButtonPress(tp, 0);
		}
		if (firstButton != nullptr && i == 0)
		{
			*firstButton = tp;
		}
	}
	return bp;
}

#if 0  // currently unused
// Create a row of icon buttons.
// Set the colours before calling this
void CreateIconButtonRow(Window * pf, PixelNumber top, PixelNumber left, PixelNumber totalWidth, PixelNumber spacing, unsigned int numButtons,
									const Icon icons[], const char* _ecv_array const params[], Event evt)
{
	const PixelNumber step = (totalWidth + spacing)/numButtons;
	for (unsigned int i = 0; i < numButtons; ++i)
	{
		pf->AddField(new IconButton(top, left + i * step, step - spacing, icons[i], evt, params[i]));
	}
}
#endif

// Create a popup bar with string parameters
PopupWindow *CreateStringPopupBar(const ColourScheme& colours, PixelNumber width, unsigned int numEntries, const char* const text[], const char* const params[], Event ev)
{
	PopupWindow *pf = new PopupWindow(popupBarHeight, width, colours.popupBackColour, colours.popupBorderColour);
	DisplayField::SetDefaultColours(colours.popupButtonTextColour, colours.popupButtonBackColour);
	PixelNumber step = (width - 2 * popupSideMargin + popupFieldSpacing)/numEntries;
	for (unsigned int i = 0; i < numEntries; ++i)
	{
		pf->AddField(new TextButton(popupTopMargin, popupSideMargin + i * step, step - popupFieldSpacing, text[i], ev, params[i]));
	}
	return pf;
}

// Create a popup bar with integer parameters
// If the 'params' parameter is null then we use 0, 1, 2.. at the parameters
PopupWindow *CreateIntPopupBar(const ColourScheme& colours, PixelNumber width, unsigned int numEntries, const char* const text[], const int * null params, Event ev, Event zeroEv)
{
	PopupWindow *pf = new PopupWindow(popupBarHeight, width, colours.popupBackColour, colours.popupBorderColour);
	DisplayField::SetDefaultColours(colours.popupButtonTextColour, colours.popupButtonBackColour);
	PixelNumber step = (width - 2 * popupSideMargin + popupFieldSpacing)/numEntries;
	for (unsigned int i = 0; i < numEntries; ++i)
	{
		const int iParam = (params == nullptr) ? (int)i : params[i];
		pf->AddField(new TextButton(popupSideMargin, popupSideMargin + i * step, step - popupFieldSpacing, text[i], (params && params[i] == 0) ? zeroEv : ev, iParam));
	}
	return pf;
}

// Nasty hack to work around bug in RepRapFirmware 1.09k and earlier
// The M23 and M30 commands don't work if we send the full path, because "0:/gcodes/" gets prepended regardless.
const char * _ecv_array StripPrefix(const char * _ecv_array dir)
{
	if (GetFirmwareFeatures().IsBitSet(noGcodesFolder))			// if running RepRapFirmware
	{
		const size_t len = strlen(dir);
		if (len >= 8 && memcmp(dir, "/gcodes/", 8) == 0)
		{
			dir += 8;
		}
		else if (len >= 10 && memcmp(dir, "0:/gcodes/", 10) == 0)
		{
			dir += 10;
		}
		else if (strcmp(dir, "/gcodes") == 0 || strcmp(dir, "0:/gcodes") == 0)
		{
			dir += len;
		}
	}
	return dir;
}


static void SendGcode(const char *data)
{
	SerialIo::Sendf("%s\n", data);
}

static void PopupEditData(const char *data)
{
	alertPopup->UpdateData(data);
	dbg("received data %s\n", data);
	mgr.ClearPopup(true, keyboardPopup);
}

#if DISPLAY_X == 800
static void M291TextData(const char *data)
{
	const size_t len = strlen(data);
	mgr.ClearPopup(true, keyboardPopup);
	keyboardIsDisplayed = false;

	if (len < (size_t)standardPopupM291TextMin || len > (size_t)standardPopupM291TextMax)
	{
		String<64> warning;
		warning.printf("Text length must be %ld..%ld characters",
			(long)standardPopupM291TextMin, (long)standardPopupM291TextMax);
		standardPopupInfoFields[3]->SetPosition(70, 183);
		standardPopupInfoFields[3]->SetValue(warning.c_str(), true);
		standardPopupInfoFields[3]->Show(true);
		mgr.Refresh(true);
		TouchBeep();
		return;
	}

	standardPopupM291ValueText.copy(data);
	standardPopupChoiceButtons[0]->SetText(standardPopupM291ValueText.c_str());
	standardPopupInfoFields[3]->Show(false);
	mgr.Refresh(true);
}
#endif

// Adjust the brightness
static void ChangeBrightness(bool up)
{
	int adjust = max<int>(1, nvData.GetBrightness() / 5);
	if (!up)
	{
		adjust = -adjust;
	}
	SetBrightness(nvData.GetBrightness() + adjust);
}


void UI::SetAxisMin(size_t index, float val)
{
	if (index >= MaxTotalAxes)
	{
		return;
	}
#if DISPLAY_X == 800
	if (!statusObjectAxisMinValid[index] || statusObjectAxisMin[index] != val) statusObjectsDirty = true;
	statusObjectAxisMin[index] = val;
	statusObjectAxisMinValid[index] = true;
#endif
}

void UI::SetAxisMax(size_t index, float val)
{
	if (index >= MaxTotalAxes)
	{
		return;
	}

	axisMaxVal = max(axisMaxVal, val);
#if DISPLAY_X == 800
	if (!statusObjectAxisMaxValid[index] || statusObjectAxisMax[index] != val) statusObjectsDirty = true;
	statusObjectAxisMax[index] = val;
	statusObjectAxisMaxValid[index] = true;
#endif
}


// Cycle through available display dimmer types
static void ChangeDisplayDimmerType()
{
	DisplayDimmerType newType = (DisplayDimmerType) ((uint8_t)nvData.GetDisplayDimmerType() + 1);
	if (newType == DisplayDimmerType::NumTypes)
	{
		newType = (DisplayDimmerType)0;
	}
	nvData.SetDisplayDimmerType(newType);
}

// Cyce through available heater combine types and repaint
static void ChangeHeaterCombineType()
{
	HeaterCombineType newType = (HeaterCombineType) ((uint8_t)nvData.GetHeaterCombineType() + 1);
	if (newType == HeaterCombineType::NumTypes)
	{
		newType = (HeaterCombineType)0;
	}
	nvData.SetHeaterCombineType(newType);
	UI::AllToolsSeen();
}

// Update an integer field, provided it isn't the one being adjusted
// Don't update it if the value hasn't changed, because that makes the display flicker unnecessarily
static void UpdateField(IntegerButton *f, int val)
{
	if (f != fieldBeingAdjusted.GetButton() && f->GetValue() != val)
	{
		f->SetValue(val);
	}
}

static void PopupAreYouSure(Event ev, const char* text, const char* query = strings->areYouSure)
{
	if (areYouSurePopup == nullptr)
	{
		return;		// legacy popup is not created in the 800x480 build; modern confirmations use the shared standard popup
	}
	eventToConfirm = ev;
	if (isLandscape)
	{
		areYouSureTextField->SetValue(text);
		areYouSureQueryField->SetValue(query);
		mgr.SetPopup(areYouSurePopup, AutoPlace, AutoPlace);
	}
}

#if DISPLAY_X != 800
static void CreateIntegerAdjustPopup(const ColourScheme& colours)
{
	// Create the popup window used to adjust temperatures, fan speed, extrusion factor etc.
	static const char* const tempPopupText[] = {"-5", "-1", strings->set, "+1", "+5"};
	static const int tempPopupParams[] = { -5, -1, 0, 1, 5 };
	setTempPopup = CreateIntPopupBar(colours, tempPopupBarWidth, 5, tempPopupText, tempPopupParams, evAdjustInt, evSetInt);
}

static void CreateIntegerRPMAdjustPopup(const ColourScheme& colours)
{
	// Create the popup window used to adjust temperatures, fan speed, extrusion factor etc.
	static const char* const rpmPopupText[] = {"-1000", "-100", "-10", strings->set, "+10", "+100", "+1000"};
	static const int rpmPopupParams[] = { -1000, -100, -10, 0, 10, 100, 1000 };
	setRPMPopup = CreateIntPopupBar(colours, rpmPopupBarWidth, 7, rpmPopupText, rpmPopupParams, evAdjustInt, evSetInt);
}
#endif

// Create the movement popup window
static void CreateMovePopup(const ColourScheme& colours)
{
#if DISPLAY_X == 800
	// The legacy move popup is not created on 800x480 (the modern MOVE page replaces it).
	// Keep the initial axis visibility that the original loop set on the other axis fields.
	UNUSED(colours);
	for (size_t i = 0; i < MaxDisplayableAxes; ++i)
	{
		UI::ShowAxis(i, i < MIN_AXES, axisNames[i]);
	}
#else
	static const char * _ecv_array const xyJogValues[] = { "-100", "-10", "-1", "-0.1", "0.1",  "1", "10", "100" };
	static const char * _ecv_array const zJogValues[] = { "-50", "-5", "-0.5", "-0.05", "0.05",  "0.5", "5", "50" };

	movePopup = new StandardPopupWindow(movePopupHeight, movePopupWidth, colours.popupBackColour, colours.popupBorderColour, colours.popupTextColour, colours.buttonImageBackColour, strings->moveHead);
	PixelNumber ypos = popupTopMargin + buttonHeight + moveButtonRowSpacing;
	const PixelNumber axisPosYpos = ypos + (MaxDisplayableAxes - 1) * (buttonHeight + moveButtonRowSpacing);
	const PixelNumber xpos = popupSideMargin + axisLabelWidth;
	PixelNumber column = popupSideMargin + margin;
	PixelNumber xyFieldWidth = (DISPLAY_X - (2 * margin) - (MaxDisplayableAxes * fieldSpacing))/(MaxDisplayableAxes + 1);

	for (size_t i = 0; i < MaxDisplayableAxes; ++i)
	{
		DisplayField::SetDefaultColours(colours.popupButtonTextColour, colours.popupButtonBackColour);
		const char * _ecv_array const * _ecv_array values = (axisNames[i][0] == 'Z') ? zJogValues : xyJogValues;
		CreateStringButtonRow(movePopup, ypos, xpos, movePopupWidth - xpos - popupSideMargin, fieldSpacing, 8, values, values, evMoveAxis, -1, true);

		// We create the label after the button row, so that the buttons follow it in the field order, which makes it easier to hide them
		DisplayField::SetDefaultColours(colours.popupTextColour, colours.popupBackColour);
		StaticTextField * const tf = new StaticTextField(ypos + labelRowAdjust, popupSideMargin, axisLabelWidth, TextAlignment::Left, axisNames[i]);
		movePopup->AddField(tf);
		moveAxisRows[i] = tf;
		UI::ShowAxis(i, i < MIN_AXES, axisNames[i]);

		DisplayField::SetDefaultColours(colours.popupTextColour, colours.popupInfoBackColour);
		FloatField *f = new FloatField(axisPosYpos, column, xyFieldWidth, TextAlignment::Left, 2, axisNames[i]);
		movePopupAxisPos[i] = f;
		movePopup->AddField(f);
		f->Show(i < MIN_AXES);
		column += xyFieldWidth + fieldSpacing;

		ypos += buttonHeight + moveButtonRowSpacing;
	}
#endif
}

#if DISPLAY_X != 800
// Create the extrusion controls popup
static void CreateExtrudePopup(const ColourScheme& colours)
{
	static const char * _ecv_array extrudeAmountValues[] = { "100", "50", "20", "10", "5",  "1" };
	static const char * _ecv_array extrudeSpeedValues[] = { "50", "20", "10", "5", "2", "1", "0.5" };
	static const char * _ecv_array extrudeSpeedParams[] = { "3000", "1200", "600", "300", "120", "60", "30" };		// must be extrudeSpeedValues * 60

	extrudePopup = new StandardPopupWindow(extrudePopupHeight, extrudePopupWidth, colours.popupBackColour, colours.popupBorderColour, colours.popupTextColour, colours.buttonImageBackColour, strings->extrusionAmount);
	PixelNumber ypos = popupTopMargin + buttonHeight + extrudeButtonRowSpacing;
	DisplayField::SetDefaultColours(colours.popupButtonTextColour, colours.popupButtonBackColour);
	currentExtrudeAmountPress = CreateStringButtonRow(extrudePopup, ypos, popupSideMargin, extrudePopupWidth - 2 * popupSideMargin, fieldSpacing, 6, extrudeAmountValues, extrudeAmountValues, evExtrudeAmount, 3);
	ypos += buttonHeight + extrudeButtonRowSpacing;
	DisplayField::SetDefaultColours(colours.popupTextColour, colours.popupBackColour);
	extrudePopup->AddField(new StaticTextField(ypos + labelRowAdjust, popupSideMargin, extrudePopupWidth - 2 * popupSideMargin, TextAlignment::Centre, strings->extrusionSpeed));
	ypos += buttonHeight + extrudeButtonRowSpacing;
	DisplayField::SetDefaultColours(colours.popupButtonTextColour, colours.popupButtonBackColour);
	currentExtrudeRatePress = CreateStringButtonRow(
			extrudePopup,
			ypos,
			popupSideMargin,
			extrudePopupWidth - 2 * popupSideMargin,
			fieldSpacing,
			ARRAY_SIZE(extrudeSpeedValues),
			extrudeSpeedValues,
			extrudeSpeedParams,
			evExtrudeRate,
			ARRAY_SIZE(extrudeSpeedValues) / 2);

	ypos += buttonHeight + extrudeButtonRowSpacing;
	extrudePopup->AddField(new TextButton(ypos, popupSideMargin, extrudePopupWidth/3 - 2 * popupSideMargin, strings->extrude, evExtrude));
	extrudePopup->AddField(new TextButton(ypos, (2 * extrudePopupWidth)/3 + popupSideMargin, extrudePopupWidth/3 - 2 * popupSideMargin, strings->retract, evRetract));
}
#endif

// Create a popup used to list files pr macros
PopupWindow *CreateFileListPopup(FileListButtons& controlButtons, TextButton ** _ecv_array fileButtons, unsigned int numRows, unsigned int numCols, const ColourScheme& colours, bool filesNotMacros,
		PixelNumber popupHeight = fileListPopupHeight, PixelNumber popupWidth = fileListPopupWidth)
pre(fileButtons.lim == numRows * numCols)
{
	PopupWindow * const popup = new StandardPopupWindow(popupHeight, popupWidth, colours.popupBackColour, colours.popupBorderColour, colours.popupTextColour, colours.buttonImageBackColour, nullptr);
	const PixelNumber closeButtonPos = popupWidth - closeButtonWidth - popupSideMargin;
	const PixelNumber navButtonWidth = (closeButtonPos - popupSideMargin)/7;
	const PixelNumber upButtonPos = closeButtonPos - navButtonWidth - fieldSpacing;
	const PixelNumber rightButtonPos = upButtonPos - navButtonWidth - fieldSpacing;
	const PixelNumber leftButtonPos = rightButtonPos - navButtonWidth - fieldSpacing;
	const PixelNumber textPos = popupSideMargin + navButtonWidth;
	const PixelNumber changeButtonPos = popupSideMargin;

	DisplayField::SetDefaultColours(colours.popupTextColour, colours.popupBackColour);
	if (filesNotMacros)
	{
		popup->AddField(filePopupTitleField = new IntegerField(popupTopMargin + labelRowAdjust, textPos, leftButtonPos - textPos, TextAlignment::Centre, strings->filesOnCard, nullptr));
		popup->AddField(fileListPopupNoFiles = new StaticTextField(popupHeight / 2 - popupTopMargin, popupSideMargin, popupWidth, TextAlignment::Centre, strings->noFilesFound));
		fileListPopupNoFiles->Show(false);
	}
	else
	{
		popup->AddField(new StaticTextField(popupTopMargin + labelRowAdjust, textPos, leftButtonPos - textPos, TextAlignment::Centre, strings->macros));
	}

	DisplayField::SetDefaultColours(colours.popupButtonTextColour, colours.buttonImageBackColour);
	if (filesNotMacros)
	{
		popup->AddField(changeCardButton = new IconButton(popupTopMargin, changeButtonPos, navButtonWidth, IconFiles, evChangeCard, 0));
	}

	const Event scrollEvent = (filesNotMacros) ? evScrollFiles : evScrollMacros;

	DisplayField::SetDefaultColours(colours.popupButtonTextColour, colours.popupButtonBackColour);
	popup->AddField(controlButtons.scrollLeftButton = new TextButton(popupTopMargin, leftButtonPos, navButtonWidth, LEFT_ARROW, scrollEvent, -1));
	controlButtons.scrollLeftButton->Show(false);
	popup->AddField(controlButtons.scrollRightButton = new TextButton(popupTopMargin, rightButtonPos, navButtonWidth, RIGHT_ARROW, scrollEvent, 1));
	controlButtons.scrollRightButton->Show(false);
	popup->AddField(controlButtons.folderUpButton = new TextButton(popupTopMargin, upButtonPos, navButtonWidth, UP_ARROW, (filesNotMacros) ? evFilesUp : evMacrosUp));
	controlButtons.folderUpButton->Show(false);

	const PixelNumber fileFieldWidth = (popupWidth + fieldSpacing - (2 * popupSideMargin))/numCols;
	for (unsigned int c = 0; c < numCols; ++c)
	{
		PixelNumber row = popupTopMargin;
		for (unsigned int r = 0; r < numRows; ++r)
		{
			row += buttonHeight + fileButtonRowSpacing;
			TextButton *t = new TextButton(row, (fileFieldWidth * c) + popupSideMargin, fileFieldWidth - fieldSpacing, nullptr, evNull);
			t->Show(false);
			popup->AddField(t);
			*fileButtons = t;
			++fileButtons;
		}
	}

	controlButtons.errorField = new IntegerField(popupTopMargin + 2 * (buttonHeight + fileButtonRowSpacing), popupSideMargin, popupWidth - (2 * popupSideMargin),
							TextAlignment::Centre, strings->error, strings->accessingSdCard);
	controlButtons.errorField->Show(false);
	popup->AddField(controlButtons.errorField);
	return popup;
}

#if DISPLAY_X != 800
static void ThumbnailRefreshNotify(bool full, bool changed)
{
	UNUSED(changed);

	if (!full || !currentFile)
		return;

	dbg("full %d changed %d currentFile %s\n", full, changed, currentFile);
	SerialIo::Sendf(GetFirmwareFeatures().IsBitSet(noM20M36) ? "M408 S36 P" : "M36 ");			// ask for the file info
	SerialIo::SendFilename(CondStripDrive(FileManager::GetFilesDir()), currentFile);
	SerialIo::SendChar('\n');
}

// Create the popup window used to display the file dialog
static void CreateFileActionPopup(const ColourScheme& colours)
{
	PixelNumber y_start, height;
	PixelNumber x_start, width;

	fileDetailPopup = new StandardPopupWindow(fileInfoPopupHeight, fileInfoPopupWidth, colours.popupBackColour, colours.popupBorderColour, colours.popupTextColour, colours.buttonImageBackColour, nullptr);
	DisplayField::SetDefaultColours(colours.popupTextColour, colours.popupBackColour);

	PixelNumber ypos = popupTopMargin + 1;
	fpNameField = new TextField(ypos, popupSideMargin, fileInfoPopupWidth - closeButtonWidth - 3 * popupSideMargin, TextAlignment::Left, strings->fileName);
	ypos += rowTextHeight + 3;
	fpGeneratedByField = new TextField(ypos, popupSideMargin, fileInfoPopupWidth - 2 * popupSideMargin, TextAlignment::Left, strings->generatedBy, generatedByText.c_str());
	ypos += rowTextHeight;

	y_start = ypos + 3;
	height = 7 * rowTextHeight + (2 * rowTextHeight) / 3;

	x_start = fileInfoPopupWidth - popupSideMargin * 3 / 2 - fileInfoPopupWidth / 3;
	width = fileInfoPopupWidth / 3 + 5;

	fpThumbnail = new DrawDirect(y_start, x_start, height, width, ThumbnailRefreshNotify);

	dbg("y_start %d x_start %d height %d width %d\n", y_start, x_start, height, width);
	dbg("text height %d\n", rowTextHeight);

	fpSizeField = new IntegerField(ypos, popupSideMargin, fileInfoPopupWidth - 2 * popupSideMargin - fileInfoPopupWidth / 3, TextAlignment::Left, strings->fileSize, " b");
	ypos += rowTextHeight;
	fpLayerHeightField = new FloatField(ypos, popupSideMargin, fileInfoPopupWidth - 2 * popupSideMargin - fileInfoPopupWidth / 3, TextAlignment::Left, 2, strings->layerHeight, "mm");
	ypos += rowTextHeight;
	fpHeightField = new FloatField(ypos, popupSideMargin, fileInfoPopupWidth - 2 * popupSideMargin - fileInfoPopupWidth / 3, TextAlignment::Left, 1, strings->objectHeight, "mm");
	ypos += rowTextHeight;
	fpFilamentField = new IntegerField(ypos, popupSideMargin, fileInfoPopupWidth - 2 * popupSideMargin - fileInfoPopupWidth / 3, TextAlignment::Left, strings->filamentNeeded, "mm");
	ypos += rowTextHeight;
	fpLastModifiedField = new TextField(ypos, popupSideMargin, fileInfoPopupWidth - 2 * popupSideMargin - fileInfoPopupWidth / 3, TextAlignment::Left, strings->lastModified, lastModifiedText.c_str());
	ypos += rowTextHeight;
	fpPrintTimeField = new TextField(ypos, popupSideMargin, fileInfoPopupWidth - 2 * popupSideMargin - fileInfoPopupWidth / 3, TextAlignment::Left, strings->estimatedPrintTime, printTimeText.c_str());
	fileDetailPopup->AddField(fpNameField);
	fileDetailPopup->AddField(fpSizeField);
	fileDetailPopup->AddField(fpLayerHeightField);
	fileDetailPopup->AddField(fpHeightField);
	fileDetailPopup->AddField(fpFilamentField);
	fileDetailPopup->AddField(fpGeneratedByField);
	fileDetailPopup->AddField(fpLastModifiedField);
	fileDetailPopup->AddField(fpPrintTimeField);
	fileDetailPopup->AddField(fpThumbnail);

	// Add the buttons
	DisplayField::SetDefaultColours(colours.popupButtonTextColour, colours.popupButtonBackColour);
	fileDetailPopup->AddField(new TextButton(popupTopMargin + 10 * rowTextHeight, popupSideMargin, fileInfoPopupWidth/3 - 2 * popupSideMargin, strings->print, evPrintFile));
	fileDetailPopup->AddField(new TextButton(popupTopMargin + 10 * rowTextHeight, fileInfoPopupWidth/3 + popupSideMargin, fileInfoPopupWidth/3 - 2 * popupSideMargin, strings->simulate, evSimulateFile));
	fileDetailPopup->AddField(new IconButton(popupTopMargin + 10 * rowTextHeight, (2 * fileInfoPopupWidth)/3 + popupSideMargin, fileInfoPopupWidth/3 - 2 * popupSideMargin, IconTrash, evDeleteFile));
}
#endif

#if DISPLAY_X != 800
// Create the "Are you sure?" popup
static void CreateAreYouSurePopup(const ColourScheme& colours)
{
	areYouSurePopup = new PopupWindow(areYouSurePopupHeight, areYouSurePopupWidth, colours.popupBackColour, colours.popupBorderColour);
	DisplayField::SetDefaultColours(colours.popupTextColour, colours.popupBackColour);
	areYouSurePopup->AddField(areYouSureTextField = new StaticTextField(popupSideMargin, margin, areYouSurePopupWidth - 2 * margin, TextAlignment::Centre, nullptr));
	areYouSurePopup->AddField(areYouSureQueryField = new StaticTextField(popupTopMargin + rowHeight, margin, areYouSurePopupWidth - 2 * margin, TextAlignment::Centre, nullptr));

	DisplayField::SetDefaultColours(colours.popupButtonTextColour, colours.popupButtonBackColour);
	areYouSurePopup->AddField(new IconButton(popupTopMargin + 2 * rowHeight, popupSideMargin, areYouSurePopupWidth/2 - 2 * popupSideMargin, IconOk, evYes));
	areYouSurePopup->AddField(new IconButton(popupTopMargin + 2 * rowHeight, areYouSurePopupWidth/2 + 10, areYouSurePopupWidth/2 - 2 * popupSideMargin, IconCancel, evCancel));
}
#endif

static void CreateScreensaverPopup()
{
	screensaverPopup = new PopupWindow(max(DisplayX, DisplayY), max(DisplayX, DisplayY), black, black, false);
	DisplayField::SetDefaultColours(white, black);
	static const char * text = "Touch to wake up";
	screensaverTextWidth = DisplayField::GetTextWidth(text, DisplayX);
	screensaverPopup->AddField(screensaverText = new StaticTextField(row1, margin, screensaverTextWidth, TextAlignment::Left, text));
}

static void CreateFirmwareUpdatePopup()
{
	firmwareUpdatePopup = new PopupWindow(max(DisplayX, DisplayY), max(DisplayX, DisplayY), black, black, false);
	DisplayField::SetDefaultColours(white, black);
	static const char * text = "Updating firmware";
	const int textWidth = DisplayField::GetTextWidth(text, DisplayX);
	firmwareUpdatePopup->AddField(new StaticTextField(DisplayY/2-rowHeight/2, DisplayX/2-textWidth/2, textWidth, TextAlignment::Left, text));
}

// Create the baud rate adjustment popup
static void CreateBaudRatePopup(const ColourScheme& colours)
{
	static const char* const baudPopupText[] = { "9600", "19200", "38400", "57600", "115200" };
	static const int baudPopupParams[] = { 9600, 19200, 38400, 57600, 115200 };
	baudPopup = CreateIntPopupBar(colours, fullPopupWidth, 5, baudPopupText, baudPopupParams, evAdjustBaudRate, evAdjustBaudRate);
}

// Create the volume adjustment popup
static void CreateVolumePopup(const ColourScheme& colours)
{
	static_assert(Buzzer::MaxVolume == 5, "MaxVolume assumed to be 5 here");
	static const char* const volumePopupText[Buzzer::MaxVolume + 1] = { "0", "1", "2", "3", "4", "5" };
	volumePopup = CreateIntPopupBar(colours, fullPopupWidth, ARRAY_SIZE(volumePopupText), volumePopupText, nullptr, evAdjustVolume, evAdjustVolume);
}

// Create the volume adjustment popup
static void CreateInfoTimeoutPopup(const ColourScheme& colours)
{
	static const char* const infoTimeoutPopupText[Buzzer::MaxVolume + 1] = { "0", "2", "5", "10" };
	static const int values[] = { 0, 2, 5, 10 };
	infoTimeoutPopup = CreateIntPopupBar(colours, fullPopupWidth, ARRAY_SIZE(infoTimeoutPopupText), infoTimeoutPopupText, values, evAdjustInfoTimeout, evAdjustInfoTimeout);
}

// Create the screensaver timeout adjustment popup
static void CreateScreensaverTimeoutPopup(const ColourScheme& colours)
{
	static const char* const screensaverTimeoutPopupText[Buzzer::MaxVolume + 1] = { "off", "60", "120", "180", "240", "300" };
	static const int values[] = { 0, 60, 120, 180, 240, 300 };
	screensaverTimeoutPopup = CreateIntPopupBar(colours, fullPopupWidth, ARRAY_SIZE(screensaverTimeoutPopupText), screensaverTimeoutPopupText, values, evAdjustScreensaverTimeout, evAdjustScreensaverTimeout);
}

// Create the babystep amount adjustment popup
static void CreateBabystepAmountPopup(const ColourScheme& colours)
{
	static const int values[] = { 0, 1, 2, 3 };
	babystepAmountPopup = CreateIntPopupBar(colours, fullPopupWidth, ARRAY_SIZE(babystepAmounts), babystepAmounts, values, evAdjustBabystepAmount, evAdjustBabystepAmount);
}

// Create the feedrate amount adjustment popup
static void CreateFeedrateAmountPopup(const ColourScheme& colours)
{
	static const char* const feedrateText[] = {"600", "1200", "2400", "6000", "12000"};
	static const int values[] = { 600, 1200, 2400, 6000, 12000 };
	feedrateAmountPopup = CreateIntPopupBar(colours, fullPopupWidth, ARRAY_SIZE(feedrateText), feedrateText, values, evAdjustFeedrate, evAdjustFeedrate);
}

// Create the colour scheme change popup
static void CreateColoursPopup(const ColourScheme& colours)
{
	if (NumColourSchemes >= 2)
	{
		// Put all the colour scheme names in a single _ecv_array for the call to CreateIntPopupBar
		const char* coloursPopupText[NumColourSchemes];
		for (size_t i = 0; i < NumColourSchemes; ++i)
		{
			coloursPopupText[i] = strings->colourSchemeNames[i];
		}
		coloursPopup = CreateIntPopupBar(colours, fullPopupWidth, NumColourSchemes, coloursPopupText, nullptr, evAdjustColours, evAdjustColours);
	}
	else
	{
		coloursPopup = nullptr;
	}
}

// Create the pop-up keyboard
//
// The geometry intentionally remains identical to the legacy 800x480 keyboard
// so the existing language, Shift, command-history and M291 text-entry logic can
// be reused. The presentation follows paneldue_keyboard_popup_mockup(1).svg.
static void CreateKeyboardPopup(ColourScheme colours)
{
	UNUSED(colours);
	static const char* _ecv_array const keysEN[8] = { "1234567890-+", "QWERTYUIOP[]", "ASDFGHJKL:@", "ZXCVBNM,./", "!\"#$%^&*()_=", "qwertyuiop{}", "asdfghjkl;'", "zxcvbnm<>?" };

	const Colour pageBg = UTFT::fromRGB(18, 22, 28);         // #12161c
	const Colour tile = UTFT::fromRGB(28, 34, 43);           // #1c222b
	const Colour text = UTFT::fromRGB(229, 232, 236);        // #e5e8ec
	const Colour neutralBorder = UTFT::fromRGB(59, 67, 79);  // #3b434f
	const Colour accent = GetModernAccentColour();        // #e2453f

	// Use flat modern button colours. The legacy CharButtonRow is retained because
	// it stores an entire 12-key row in one object, saving RAM compared with 48
	// individual key objects.
	DisplayField::SetDefaultColours(text, tile, neutralBorder, tile, tile, tile, IconPaletteDark);
	keyboardPopup = new StandardPopupWindow(keyboardPopupHeight, keyboardPopupWidth,
		pageBg, accent, text, tile, nullptr, keyboardTopMargin);

	// Input card from the SVG. The TextField sits on top of the card so all of the
	// existing console/M291 text buffer handling remains unchanged.
	DisplayField::SetDefaultColours(text, tile);
	userCommandField = new TextField(keyboardTopMargin + labelRowAdjust, popupSideMargin + 8,
		keyboardPopupWidth - 2 * popupSideMargin - closeButtonWidth - popupFieldSpacing - 8,
		TextAlignment::Left, nullptr, "_");
	userCommandField->SetLabel(userCommandBuffers[currentUserCommandBuffer].c_str());
	keyboardPopup->AddField(userCommandField);
	keyboardPopup->AddField(new ModernCard(keyboardTopMargin + labelRowAdjust, popupSideMargin,
		keyboardPopupWidth - 2 * popupSideMargin - closeButtonWidth - popupFieldSpacing,
		rowTextHeight, tile, neutralBorder, true));

	currentKeyboard = keysEN;
	PixelNumber row = keyboardTopMargin + keyButtonVStep;

	// Flat dark key tiles with neutral outlines. The positions are unchanged and
	// therefore match the uploaded 800x324 keyboard SVG exactly.
	DisplayField::SetDefaultColours(text, tile, neutralBorder, tile, tile, tile, IconPaletteDark);
	for (size_t i = 0; i < 4; ++i)
	{
		const PixelNumber column = popupSideMargin + (i * keyButtonHStep)/3;
		keyboardRows[i] = new CharButtonRow(row, column, keyButtonWidth, keyButtonHStep, currentKeyboard[i], evKey);
		keyboardPopup->AddField(keyboardRows[i]);
		switch (i)
		{
		case 0:
			keyboardPopup->AddField(new IconButton(row, keyboardPopupWidth - popupSideMargin - (5 * keyButtonWidth)/4,
				(5 * keyButtonWidth)/4, IconBackspace, evBackspace));
			break;

		case 2:
			keyboardPopup->AddField(new TextButton(row, keyboardPopupWidth - popupSideMargin - (3 * keyButtonWidth)/2,
				(3 * keyButtonWidth)/2, UP_ARROW, evUp));
			break;

		case 3:
			keyboardPopup->AddField(new TextButton(row, keyboardPopupWidth - popupSideMargin - (3 * keyButtonWidth)/2,
				(3 * keyButtonWidth)/2, DOWN_ARROW, evDown));
			break;

		default:
			break;
		}
		row += keyButtonVStep;
	}

	// Shift, Space and Enter preserve the existing behavior and exact source/SVG
	// geometry. Shift remains sticky via the existing evShift handler.
	const PixelNumber keyButtonHSpace = keyButtonHStep - keyButtonWidth;
	const PixelNumber wideKeyButtonWidth = (keyboardPopupWidth - 2 * popupSideMargin - 2 * keyButtonHSpace)/5;
	keyboardPopup->AddField(new TextButton(row, popupSideMargin, wideKeyButtonWidth, "Shift", evShift, 0));
	keyboardPopup->AddField(new TextButton(row, popupSideMargin + wideKeyButtonWidth + keyButtonHSpace,
		2 * wideKeyButtonWidth, "", evKey, (int)' '));
	keyboardPopup->AddField(new IconButton(row, popupSideMargin + 3 * wideKeyButtonWidth + 2 * keyButtonHSpace,
		wideKeyButtonWidth, IconEnter, evSendKeyboardCommand));

	// Add the inset frames last. Window::AddField prepends fields, so these render
	// before the controls and form the SVG's 4 px Accent frame plus neutral inset.
	keyboardPopup->AddField(new ModernCard(4, 4, keyboardPopupWidth - 8, keyboardPopupHeight - 8,
		pageBg, neutralBorder, true));
	keyboardPopup->AddField(new ModernCard(2, 2, keyboardPopupWidth - 4, keyboardPopupHeight - 4,
		pageBg, accent, true));

	keyboardDataHandler = SendGcode;

	// Do not leak the keyboard-specific flat button defaults into popups created
	// later in CreateFields().
	DisplayField::SetDefaultColours(colours.buttonTextColour, colours.buttonTextBackColour,
		colours.buttonBorderColour, colours.buttonGradColour, colours.buttonPressedBackColour,
		colours.buttonPressedGradColour, colours.pal);
}

#if DISPLAY_X != 800
// Create the babystep popup
static void CreateBabystepPopup(const ColourScheme& colours)
{
	babystepPopup = new StandardPopupWindow(babystepPopupHeight, babystepPopupWidth, colours.popupBackColour, colours.popupBorderColour, colours.popupTextColour, colours.buttonImageBackColour,
			strings->babyStepping);
	PixelNumber ypos = popupTopMargin + babystepRowSpacing;
	DisplayField::SetDefaultColours(colours.popupTextColour, colours.popupBackColour);
	babystepPopup->AddField(babystepOffsetField = new FloatField(ypos, popupSideMargin, babystepPopupWidth - 2 * popupSideMargin, TextAlignment::Left, 3, strings->currentZoffset, "mm"));
	ypos += babystepRowSpacing;
	DisplayField::SetDefaultColours(colours.popupTextColour, colours.buttonImageBackColour);
	const PixelNumber width = CalcWidth(2, babystepPopupWidth - 2 * popupSideMargin);
	babystepPopup->AddField(babystepMinusButton = new TextButtonWithLabel(ypos, CalcXPos(0, width, popupSideMargin), width, babystepAmounts[nvData.GetBabystepAmountIndex()], evBabyStepMinus, nullptr, LESS_ARROW " "));
	babystepPopup->AddField(babystepPlusButton = new TextButtonWithLabel(ypos, CalcXPos(1, width, popupSideMargin), width, babystepAmounts[nvData.GetBabystepAmountIndex()], evBabyStepPlus, nullptr, MORE_ARROW " "));
}
#endif

// Create the grid of heater icons and temperatures
static void CreateTemperatureGrid(const ColourScheme& colours)
{
	// Add the emergency stop button
	DisplayField::SetDefaultColours(colours.stopButtonTextColour, colours.stopButtonBackColour);
	mgr.AddField(new TextButton(row2, margin, bedColumn - fieldSpacing - margin - 16, strings->stop, evEmergencyStop));

	// Add the labels and the debug field
	DisplayField::SetDefaultColours(colours.labelTextColour, colours.defaultBackColour);
	mgr.AddField(debugField = new StaticTextField(row1 + labelRowAdjust, margin, bedColumn - fieldSpacing - margin, TextAlignment::Left, "debug"));
	mgr.AddField(new StaticTextField(row3 + labelRowAdjust, margin, bedColumn - fieldSpacing - margin, TextAlignment::Right, strings->current));
	mgr.AddField(new StaticTextField(row4 + labelRowAdjust, margin, bedColumn - fieldSpacing - margin, TextAlignment::Right, strings->active));
	mgr.AddField(new StaticTextField(row5 + labelRowAdjust, margin, bedColumn - fieldSpacing - margin, TextAlignment::Right, strings->standby));

	// Add the grid
	for (unsigned int i = 0; i < MaxSlots; ++i)
	{
		const PixelNumber column = ((tempButtonWidth + fieldSpacing) * i) + bedColumn;

		// Add the icon button
		DisplayField::SetDefaultColours(colours.buttonTextColour, colours.buttonImageBackColour);
		IconButtonWithText * const b = new IconButtonWithText(row2, column, tempButtonWidth, i == 0 ? IconBed : IconNozzle, evSelectHead, i, i);
		b->Show(false);
		toolButtons[i] = b;
		mgr.AddField(b);

		// Add the current temperature field
		DisplayField::SetDefaultColours(colours.infoTextColour, colours.defaultBackColour);
		FloatField * const f = new FloatField(row3 + labelRowAdjust, column, tempButtonWidth, TextAlignment::Centre, 1);
		f->Show(false);
		currentTemps[i] = f;
		mgr.AddField(f);

		// Add the active temperature button
		DisplayField::SetDefaultColours(colours.buttonTextColour, colours.buttonTextBackColour);
		IntegerButton *ib = new IntegerButton(row4, column, tempButtonWidth);
		ib->SetEvent(evAdjustToolActiveTemp, i);
		ib->SetValue(0);
		ib->Show(false);
		activeTemps[i] = ib;
		mgr.AddField(ib);

		// Add the standby temperature button
		ib = new IntegerButton(row5, column, tempButtonWidth);
		ib->SetEvent(evAdjustToolStandbyTemp, i);
		ib->SetValue(0);
		ib->Show(false);
		standbyTemps[i] = ib;
		mgr.AddField(ib);
	}
}

// Create the extra fields for the Control tab
static void CreateControlTabFields(const ColourScheme& colours)
{
	mgr.SetRoot(commonRoot);

	DisplayField::SetDefaultColours(colours.infoTextColour, colours.infoBackColour);
	PixelNumber column = margin;
	PixelNumber xyFieldWidth = (DISPLAY_X - (2 * margin) - (MaxDisplayableAxes * fieldSpacing))/(MaxDisplayableAxes + 1);
	for (size_t i = 0; i < MaxDisplayableAxes; ++i)
	{
		FloatField * const f = new FloatField(row6p3 + labelRowAdjust, column, xyFieldWidth, TextAlignment::Left, 2, axisNames[i]);
		controlTabAxisPos[i] = f;
		mgr.AddField(f);
		f->Show(i < MIN_AXES);
		column += xyFieldWidth + fieldSpacing;
	}
	zprobeBuf[0] = 0;
	mgr.AddField(zProbe = new TextField(row6p3 + labelRowAdjust, column, DISPLAY_X - column - margin, TextAlignment::Left, "P", zprobeBuf.c_str()));

	DisplayField::SetDefaultColours(colours.buttonTextColour, colours.notHomedButtonBackColour);
	homeAllButton = AddIconButton(row7p7, 0, MaxDisplayableAxes + 2, IconHomeAll, evSendCommand, "G28");
	homeButtons[0] = AddIconButtonWithText(row7p7, 1, MaxDisplayableAxes + 2, IconHomeAll, evHomeAxis, axisNames[0], axisNames[0]);
	homeButtons[1] = AddIconButtonWithText(row7p7, 2, MaxDisplayableAxes + 2, IconHomeAll, evHomeAxis, axisNames[1], axisNames[1]);
	homeButtons[2] = AddIconButtonWithText(row7p7, 3, MaxDisplayableAxes + 2, IconHomeAll, evHomeAxis, axisNames[2], axisNames[2]);
#if MaxDisplayableAxes > 3
	homeButtons[3] = AddIconButtonWithText(row7p7, 4, MaxDisplayableAxes + 2, IconHomeAll, evHomeAxis, axisNames[3], axisNames[3]);
	homeButtons[3]->Show(false);
#endif
#if MaxDisplayableAxes > 4
	homeButtons[4] = AddIconButtonWithText(row7p7, 5, MaxDisplayableAxes + 2, IconHomeAll, evHomeAxis, axisNames[4], axisNames[4]);
	homeButtons[4]->Show(false);
#endif
#if MaxDisplayableAxes > 5
	homeButtons[5] = AddIconButtonWithText(row7p7, 6, MaxDisplayableAxes + 2, IconHomeAll, evHomeAxis, axisNames[5], axisNames[5]);
	homeButtons[5]->Show(false);
#endif
	DisplayField::SetDefaultColours(colours.buttonTextColour, colours.buttonImageBackColour);
	bedCompButton = AddIconButton(row7p7, MaxDisplayableAxes + 1, MaxDisplayableAxes + 2, IconBedComp, evSendCommand, "G32");

	filesButton = AddIconButton(row8p7, 0, 4, IconFiles, evListFiles, nullptr);
	DisplayField::SetDefaultColours(colours.buttonTextColour, colours.buttonTextBackColour);
	moveButton = AddTextButton(row8p7, 1, 4, strings->move, evMovePopup, nullptr);
	extrudeButton = AddTextButton(row8p7, 2, 4, strings->extrusion, evExtrudePopup, nullptr);
	macroButton = AddTextButton(row8p7, 3, 4, strings->macro, evListMacros, nullptr);

	// When there is room, we also display a few macro buttons on the right hand side
	for (size_t i = 0; i < NumControlPageMacroButtons; ++i)
	{
		// The position and width of the buttons will get corrected when we know how many tools we have
		TextButton * const b = controlPageMacroButtons[i] = new TextButton(row2 + i * rowHeight, 999, 99, nullptr, evNull);
		b->Show(false);			// hide them until we have loaded the macros
		mgr.AddField(b);
	}

	controlRoot = mgr.GetRoot();
}

// Create the fields for the Printing tab
static void CreatePrintingTabFields(const ColourScheme& colours)
{
	mgr.SetRoot(commonRoot);

	DisplayField::SetDefaultColours(colours.buttonTextColour, colours.buttonTextBackColour);
	mgr.AddField(new TextButton(row6, margin, bedColumn - fieldSpacing - margin, "OBJECTS", evStatusObjects));

	// Extrusion factor buttons
	DisplayField::SetDefaultColours(colours.buttonTextColour, colours.buttonTextBackColour);
	for (unsigned int i = 0; i < MaxSlots; ++i)
	{
		const PixelNumber column = ((tempButtonWidth + fieldSpacing) * i) + bedColumn;

		IntegerButton * const ib = new IntegerButton(row6, column, tempButtonWidth);
		ib->SetValue(100);
		ib->SetEvent(evExtrusionFactor, i);
		ib->Show(false);
		extrusionFactors[i] = ib;
		mgr.AddField(ib);
	}

	// Speed button
	mgr.AddField(spd = new IntegerButton(row7, speedColumn, stateColumnWdith - fieldSpacing, strings->speed, "%"));
	spd->SetValue(100);
	spd->SetEvent(evAdjustSpeed, "M220 S");

	// Fan button
	mgr.AddField(fanSpeed = new IntegerButton(row7, fanColumn, stateColumnWdith - fieldSpacing, strings->fan, "%"));
	fanSpeed->SetEvent(evAdjustFan, 0);
	fanSpeed->SetValue(0);

	DisplayField::SetDefaultColours(colours.buttonTextColour, colours.buttonTextBackColour);
	babystepButton = new TextButton(row7, babystepColumn, stateColumnWdith - fieldSpacing, strings->babystep, evBabyStepPopup);
	mgr.AddField(babystepButton);

	DisplayField::SetDefaultColours(colours.buttonTextColour, colours.resetButtonBackColour);
	cancelButton = new TextButton(row7, cancelColumn, stateColumnWdith - fieldSpacing, strings->cancel, evReset, "M0");
	mgr.AddField(cancelButton);

	DisplayField::SetDefaultColours(colours.buttonTextColour, colours.pauseButtonBackColour);
	pauseButton = new TextButton(row7, pauseColumn, stateColumnWdith - (2 * margin), strings->pause, evPausePrint, "M25");
	mgr.AddField(pauseButton);

	DisplayField::SetDefaultColours(colours.buttonTextColour, colours.resumeButtonBackColour);
	resumeButton = new TextButton(row7, resumeColumn, stateColumnWdith - (2 * margin), strings->resume, evResumePrint, "M24");
	mgr.AddField(resumeButton);

#if DISPLAY_X == 800
	// On 5" and 7" screens there is room to show the current position on the Print page
	const PixelNumber offset = rowHeight - 20;
	DisplayField::SetDefaultColours(colours.infoTextColour, colours.infoBackColour);
	PixelNumber column = margin;
	PixelNumber xyFieldWidth = (DISPLAY_X - (2 * margin) - (MaxDisplayableAxes * fieldSpacing))/(MaxDisplayableAxes + 1);
	for (size_t i = 0; i < MaxDisplayableAxes; ++i)
	{
		FloatField * const f = new FloatField(row8 + labelRowAdjust - 4, column, xyFieldWidth, TextAlignment::Left, 2, axisNames[i]);
		printTabAxisPos[i] = f;
		mgr.AddField(f);
		f->Show(i < MIN_AXES);
		column += xyFieldWidth + fieldSpacing;
	}
#else
	const PixelNumber offset = 0;
#endif

	DisplayField::SetDefaultColours(colours.buttonTextColour, colours.buttonTextBackColour);
	const PixelNumber reprintRow =
#if DISPLAY_X == 800
			row9
#else
			row8
#endif
			;
	reprintButton = new TextButton(reprintRow, speedColumn, 2 * stateColumnWdith - fieldSpacing, strings->reprint, evReprint);
	reprintButton->Show(false);
	mgr.AddField(reprintButton);

	DisplayField::SetDefaultColours(colours.progressBarColour,colours. progressBarBackColour);
	mgr.AddField(printProgressBar = new ProgressBar(row8 + offset + (rowHeight - progressBarHeight)/2, margin, progressBarHeight, DisplayX - 2 * margin));
	mgr.Show(printProgressBar, false);

	DisplayField::SetDefaultColours(colours.labelTextColour, colours.defaultBackColour);
	mgr.AddField(timeLeftField = new TextField(row9 + offset, margin, DisplayX - 2 * margin, TextAlignment::Left, strings->timeRemaining));
	mgr.Show(timeLeftField, false);

	printRoot = mgr.GetRoot();
}

static void AddStatusSubTabs(DisplayField *&root);
#if DISPLAY_X == 800
static PixelNumber ObjectX(PixelNumber svgX);
static PixelNumber ObjectW(PixelNumber svgW);
#endif

// Create the Status > Object subpage.
#if DISPLAY_X == 800
static void GetStatusObjectDisplayName(unsigned int index, String<32>& out)
{
	if (index < StatusMaxObjects && !statusObjects[index].name.IsEmpty())
	{
		out.copy(statusObjects[index].name.c_str());
	}
	else
	{
		out.printf("Object %u", index + 1);
	}
}

static bool GetStatusObjectBedBounds(float& xMin, float& xMax, float& yMin, float& yMax)
{
	if (statusObjectXAxis < 0 || statusObjectYAxis < 0 ||
		statusObjectXAxis >= static_cast<int>(MaxTotalAxes) || statusObjectYAxis >= static_cast<int>(MaxTotalAxes) ||
		!statusObjectAxisMinValid[statusObjectXAxis] || !statusObjectAxisMaxValid[statusObjectXAxis] ||
		!statusObjectAxisMinValid[statusObjectYAxis] || !statusObjectAxisMaxValid[statusObjectYAxis])
	{
		return false;
	}
	xMin = statusObjectAxisMin[statusObjectXAxis];
	xMax = statusObjectAxisMax[statusObjectXAxis];
	yMin = statusObjectAxisMin[statusObjectYAxis];
	yMax = statusObjectAxisMax[statusObjectYAxis];
	return xMax > xMin && yMax > yMin;
}

static uint32_t FloatBits(float f)
{
	uint32_t u;
	memcpy(&u, &f, sizeof(u));
	return u;
}

static uint32_t statusObjectMapSignature = 0;
static bool statusObjectMapSignatureValid = false;

static void RefreshStatusObjectMap()
{
	if (statusObjectMap == nullptr)
	{
		return;
	}

	// Everything that decides what the map looks like, folded into one number. If it is the same as the last time the map
	// was drawn there is nothing to do, and repainting the map anyway is what made it blink on every job update.
	{
		float sxMin = 0.0f, sxMax = 0.0f, syMin = 0.0f, syMax = 0.0f;
		const bool sBounds = GetStatusObjectBedBounds(sxMin, sxMax, syMin, syMax);
		uint32_t sig = 2166136261u;
		const auto mix = [&sig](uint32_t v) { sig = (sig ^ v) * 16777619u; };
		mix(sBounds ? 1u : 0u);
		mix(FloatBits(sxMin)); mix(FloatBits(sxMax)); mix(FloatBits(syMin)); mix(FloatBits(syMax));
		mix(statusObjectCount);
		mix(static_cast<uint32_t>(selectedStatusObject + 1));
		for (unsigned int i = 0; i < StatusMaxObjects && i < statusObjectCount; ++i)
		{
			const StatusObjectInfo& o = statusObjects[i];
			mix((o.present ? 1u : 0u) | (o.cancelled ? 2u : 0u) | (o.xValid ? 4u : 0u) | (o.yValid ? 8u : 0u));
			mix(FloatBits(o.xMin)); mix(FloatBits(o.xMax)); mix(FloatBits(o.yMin)); mix(FloatBits(o.yMax));
		}
		if (statusObjectMapSignatureValid && sig == statusObjectMapSignature)
		{
			return;
		}
		statusObjectMapSignature = sig;
		statusObjectMapSignatureValid = true;
	}

	const Colour tile = UTFT::fromRGB(28, 34, 43);
	const Colour neutral = UTFT::fromRGB(195, 202, 212);
	const Colour accent = GetModernAccentColour();
	const Colour cancelledFill = UTFT::fromRGB(201, 50, 24);
	const Colour cancelledText = UTFT::fromRGB(36, 36, 36);

	float xMin = 0.0f, xMax = 0.0f, yMin = 0.0f, yMax = 0.0f;
	const bool boundsValid = GetStatusObjectBedBounds(xMin, xMax, yMin, yMax);
	statusObjectMap->SetBounds(xMin, xMax, yMin, yMax, boundsValid);
	// Clearing the canvas here also removes markers that moved or disappeared.
	statusObjectMap->SetChanged();

	PixelNumber bedX = 0, bedY = 0, bedW = 0, bedH = 0;
	statusObjectMap->GetBedBounds(bedX, bedY, bedW, bedH);
	PixelNumber placedX[StatusMaxObjects] = { 0 };
	PixelNumber placedY[StatusMaxObjects] = { 0 };
	unsigned int placedCount = 0;
	static const int8_t jitterX[] = { 0, 10, -10, 10, -10, 16, -16, 0, 0 };
	static const int8_t jitterY[] = { 0, -10, 10, 10, -10, 0, 0, 16, -16 };

	for (unsigned int i = 0; i < StatusMaxObjects; ++i)
	{
		ModernTextButton * const marker = statusObjectMarkers[i];
		if (marker == nullptr)
		{
			continue;
		}
		const StatusObjectInfo& obj = statusObjects[i];
		if (i >= statusObjectCount || !obj.present || !obj.xValid || !obj.yValid || !boundsValid)
		{
			mgr.Show(marker, false);
			continue;
		}

		const float objectX = (obj.xMin + obj.xMax) * 0.5f;
		const float objectY = (obj.yMin + obj.yMax) * 0.5f;
		PixelNumber centreX = 0, centreY = 0;
		if (!statusObjectMap->Project(objectX, objectY, centreX, centreY))
		{
			mgr.Show(marker, false);
			continue;
		}

		// Apply a very small deterministic offset when two numbered markers overlap.
		int bestX = static_cast<int>(centreX);
		int bestY = static_cast<int>(centreY);
		for (unsigned int attempt = 0; attempt < ARRAY_SIZE(jitterX); ++attempt)
		{
			const int candidateX = static_cast<int>(centreX) + jitterX[attempt];
			const int candidateY = static_cast<int>(centreY) + jitterY[attempt];
			bool overlaps = false;
			for (unsigned int p = 0; p < placedCount; ++p)
			{
				const int dx = candidateX - static_cast<int>(placedX[p]);
				const int dy = candidateY - static_cast<int>(placedY[p]);
				if (dx > -28 && dx < 28 && dy > -28 && dy < 28)
				{
					overlaps = true;
					break;
				}
			}
			bestX = candidateX;
			bestY = candidateY;
			if (!overlaps)
			{
				break;
			}
		}

		const int markerSize = 32;
		int left = bestX - markerSize / 2;
		int top = bestY - markerSize / 2;
		const int minLeft = static_cast<int>(bedX);
		const int maxLeft = static_cast<int>(bedX + bedW) - markerSize;
		const int minTop = static_cast<int>(bedY);
		const int maxTop = static_cast<int>(bedY + bedH) - markerSize;
		if (left < minLeft) left = minLeft;
		if (left > maxLeft) left = maxLeft;
		if (top < minTop) top = minTop;
		if (top > maxTop) top = maxTop;
		if (left < 0) left = 0;
		if (top < 0) top = 0;
		marker->SetPosition(static_cast<PixelNumber>(left), static_cast<PixelNumber>(top));
		statusObjectMarkerText[i].printf("%u", i + 1);
		marker->SetText(statusObjectMarkerText[i].c_str());

		const bool selected = selectedStatusObject == static_cast<int>(i);
		if (obj.cancelled)
		{
			marker->SetColours(cancelledText, cancelledFill);
			marker->SetBorderVisible(false);
		}
		else
		{
			marker->SetColours(selected ? accent : neutral, tile);
			marker->SetBorderVisible(true);
			marker->SetBorderColour(selected ? accent : neutral);
		}
		// Mark the marker visible and changed, but do NOT draw it now (mgr.Show(marker, true) would): the map is redrawn by the
		// next mgr.Refresh() and would paint over a marker drawn earlier. In the refresh pass the map comes first, then the markers.
		marker->Show(true);
		marker->SetChanged();
		placedX[placedCount] = static_cast<PixelNumber>(bestX);
		placedY[placedCount] = static_cast<PixelNumber>(bestY);
		++placedCount;
	}
}

static void RefreshStatusObjectRows()
{
	const Colour tile = UTFT::fromRGB(28, 34, 43);
	const Colour text = UTFT::fromRGB(229, 232, 236);
	const Colour accent = GetModernAccentColour();
	const Colour cancelledFill = UTFT::fromRGB(201, 50, 24);
	const Colour cancelledText = UTFT::fromRGB(36, 36, 36);

	for (unsigned int row = 0; row < StatusObjectsPerPage; ++row)
	{
		const unsigned int index = statusObjectPage * StatusObjectsPerPage + row;
		ModernTextButton * const number = statusObjectNumberButtons[row];
		ModernTextButton * const name = statusObjectNameButtons[row];
		if (index >= statusObjectCount || index >= StatusMaxObjects || !statusObjects[index].present)
		{
			mgr.Show(number, false);
			mgr.Show(name, false);
			continue;
		}

		statusObjectRowNumberText[row].printf("%u", index + 1);
		GetStatusObjectDisplayName(index, statusObjectRowNameText[row]);
		number->SetText(statusObjectRowNumberText[row].c_str());
		name->SetText(statusObjectRowNameText[row].c_str());
		const bool selected = selectedStatusObject == static_cast<int>(index);
		const bool cancelled = statusObjects[index].cancelled;
		number->SetEvent(cancelled ? evNull : evStatusObjectNumber, static_cast<int>(row));

		if (cancelled)
		{
			number->SetColours(cancelledText, cancelledFill);
			number->SetBorderVisible(false);       // semantic red takes precedence over Accent
		}
		else
		{
			number->SetColours(selected ? accent : text, tile);
			number->SetBorderVisible(selected);
			number->SetBorderColour(accent);
		}
		name->SetColours(text, tile);
		name->SetBorderVisible(selected);
		name->SetBorderColour(accent);
		mgr.Show(number, true);
		mgr.Show(name, true);
	}

	mgr.Show(statusObjectPageUpButton, statusObjectPage > 0);
	mgr.Show(statusObjectPageDownButton,
		(statusObjectPage + 1) * StatusObjectsPerPage < statusObjectCount &&
		(statusObjectPage + 1) * StatusObjectsPerPage < StatusMaxObjects);
}

static void RefreshStatusObjectsPage()
{
	const unsigned int maxPage = (statusObjectCount == 0) ? 0 : (statusObjectCount - 1) / StatusObjectsPerPage;
	if (statusObjectPage > maxPage)
	{
		statusObjectPage = maxPage;
		statusObjectsNeedFullRefresh = true;
	}
	if (selectedStatusObject >= static_cast<int>(statusObjectCount))
	{
		selectedStatusObject = -1;
	}
	RefreshStatusObjectRows();
	RefreshStatusObjectMap();
}

static void SelectStatusObject(unsigned int index, bool revealPage)
{
	if (index >= statusObjectCount || index >= StatusMaxObjects || !statusObjects[index].present)
	{
		return;
	}
	const unsigned int oldPage = statusObjectPage;
	selectedStatusObject = static_cast<int>(index);
	if (revealPage)
	{
		statusObjectPage = index / StatusObjectsPerPage;
	}
	RefreshStatusObjectsPage();
	if (statusObjectPage != oldPage)
	{
		mgr.Refresh(true);
	}
	else
	{
		mgr.Refresh(false);
	}
}

static void OpenStatusObjectCancelPopup(unsigned int index)
{
	if (index >= statusObjectCount || index >= StatusMaxObjects || !statusObjects[index].present || statusObjects[index].cancelled)
	{
		return;
	}
	pendingStatusObjectCancel = static_cast<int>(index);
	selectedStatusObject = static_cast<int>(index);
	String<32> displayName;
	GetStatusObjectDisplayName(index, displayName);
	statusObjectCancelNameText.printf("%u  %s", index + 1, displayName.c_str());
	RefreshStatusObjectsPage();
	mgr.Refresh(false);
	standardPopupContext = StandardPopupContext::StatusObjectCancel;
	ConfigureStandardPopupTitle("CANCEL OBJECT", true);
	ResetStandardPopupContent();
	ConfigureStandardPopupInformation("Do you want to cancel printing this object only?", statusObjectCancelNameText.c_str());
	standardPopupCancelButton->Show(true);
	standardPopupConfirmButton->Show(true);
	mgr.SetPopup(standardPopup, AutoPlace, AutoPlace);
}

#endif

static void CreateStatusObjectsTabFields(const ColourScheme& colours)
{
#if DISPLAY_X == 800
	UNUSED(colours);
	mgr.SetRoot(baseRoot);
	const Colour pageBg = UTFT::fromRGB(18, 22, 28);
	const Colour tile = UTFT::fromRGB(28, 34, 43);
	const Colour text = UTFT::fromRGB(229, 232, 236);
	const Colour neutral = UTFT::fromRGB(195, 202, 212);
	const Colour mapBorder = UTFT::fromRGB(59, 67, 79);
	const Colour axes = UTFT::fromRGB(90, 100, 114);

	DisplayField::SetDefaultFont(glcd19x21);
	DisplayField::SetDefaultColours(text, tile);
	for (unsigned int row = 0; row < StatusObjectsPerPage; ++row)
	{
		const PixelNumber y = 85 + row * 62;
		statusObjectNumberButtons[row] = new ModernTextButton(y, ObjectX(118), ObjectW(55), 56, "", evStatusObjectNumber, row, glcd19x21);
		mgr.AddField(statusObjectNumberButtons[row]);
		statusObjectNameButtons[row] = new ModernTextButton(y, ObjectX(181), ObjectW(217), 56, "", evStatusObjectSelect, row, glcd19x21, false, TextAlignment::Left);
		mgr.AddField(statusObjectNameButtons[row]);
		mgr.Show(statusObjectNumberButtons[row], false);
		mgr.Show(statusObjectNameButtons[row], false);
	}

	statusObjectPageUpButton = new ModernIconButton(401, ObjectX(118), ObjectW(137), 46, IconUp, evStatusObjectPageUp);
	statusObjectPageDownButton = new ModernIconButton(401, ObjectX(261), ObjectW(137), 46, IconDown, evStatusObjectPageDown);
	mgr.AddField(statusObjectPageUpButton);
	mgr.AddField(statusObjectPageDownButton);
	mgr.Show(statusObjectPageUpButton, false);
	mgr.Show(statusObjectPageDownButton, false);

	DisplayField::SetDefaultColours(neutral, tile);
	for (unsigned int i = 0; i < StatusMaxObjects; ++i)
	{
		statusObjectMarkerText[i].printf("%u", i + 1);
		statusObjectMarkers[i] = new ModernTextButton(110, ObjectX(472), 32, 32,
			statusObjectMarkerText[i].c_str(), evStatusObjectMarker, i, glcd19x21, true);
		statusObjectMarkers[i]->SetBorderColour(neutral);
		mgr.AddField(statusObjectMarkers[i]);
		mgr.Show(statusObjectMarkers[i], false);
	}

	statusObjectMap = new StatusObjectMapField(75, ObjectX(440), ObjectW(340), 370,
		ObjectX(472), 110, ObjectW(280), 280, pageBg, tile, mapBorder, axes);
	mgr.AddField(statusObjectMap);

	statusObjectsRoot = mgr.GetRoot();
	AddStatusSubTabs(statusObjectsRoot);
	mgr.SetRoot(statusObjectsRoot);
	mgr.AddField(new ModernCard(0, masterTabWidth, DisplayX - masterTabWidth, DisplayY, pageBg, pageBg));
	statusObjectsRoot = mgr.GetRoot();
	DisplayField::SetDefaultFont(DEFAULT_FONT);
	RefreshStatusObjectsPage();
#else
    mgr.SetRoot(baseRoot);

    const PixelNumber listLeft = contentLeft + margin;
    const PixelNumber listWidth = contentWidth * 4 / 10;
    const PixelNumber mapLeft = listLeft + listWidth + margin;
    const PixelNumber mapWidth = DisplayX - mapLeft - margin;

    DisplayField::SetDefaultColours(colours.infoTextColour, colours.defaultBackColour);
    mgr.AddField(new StaticTextField(contentTop + margin, listLeft, listWidth, "OBJECT CANCEL", TextAlignment::Left));

    const PixelNumber firstRow = contentTop + buttonHeight + margin;
    const PixelNumber rowHeight = buttonHeight;
    mgr.AddField(new TextButton(firstRow, listLeft, listWidth, "1  Object 1", evStatusObject1));
    mgr.AddField(new TextButton(firstRow + rowHeight, listLeft, listWidth, "2  Object 2", evStatusObject2));
    mgr.AddField(new TextButton(firstRow + 2 * rowHeight, listLeft, listWidth, "3  Object 3", evStatusObject3));
    mgr.AddField(new TextButton(firstRow + 3 * rowHeight, listLeft, listWidth, "4  Object 4", evStatusObject4));
    mgr.AddField(new TextButton(firstRow + 4 * rowHeight, listLeft, listWidth, "5  Object 5", evStatusObject5));
    mgr.AddField(new TextButton(firstRow + 5 * rowHeight, listLeft, listWidth, "6  Object 6", evStatusObject6));
    mgr.AddField(new TextButton(DisplayY - buttonHeight - margin, listLeft, (listWidth - margin) / 2, "UP", evStatusObjectPageUp));
    mgr.AddField(new TextButton(DisplayY - buttonHeight - margin, listLeft + (listWidth + margin) / 2, (listWidth - margin) / 2, "DOWN", evStatusObjectPageDown));

    DisplayField::SetDefaultColours(colours.infoTextColour, colours.defaultBackColour);
    mgr.AddField(new StaticTextField(contentTop + margin, mapLeft, mapWidth, "TOP VIEW", TextAlignment::Centre));
    statusObjectsRoot = mgr.GetRoot();
#endif
}

// Create the fields for the Message/Console tab
static void CreateMessageTabFields(const ColourScheme& colours)
{
	mgr.SetRoot(baseRoot);
#if DISPLAY_X == 800
	UNUSED(colours);
	// Modern SYSTEM > CONSOLE. Keep the proven legacy message-log geometry and
	// behaviour, but use the modern dark palette and remove the redundant
	// "Messages" heading because the CONSOLE sub-tab already identifies the page.
	const Colour pageBg = UTFT::fromRGB(18, 22, 28);        // #12161c
	const Colour tile = UTFT::fromRGB(28, 34, 43);          // #1c222b
	const Colour text = UTFT::fromRGB(229, 232, 236);       // #e5e8ec
	const Colour accent = GetModernAccentColour();       // #e2453f

	DisplayField::SetDefaultColours(text, tile, accent, tile, tile, tile, IconPaletteDark);
	ModernIconButton * const consoleKeyboardButton = new ModernIconButton(
		margin, DisplayX - margin - keyboardButtonWidth, keyboardButtonWidth, buttonHeight,
		IconKeyboard, evKeyboard, 0, true);
	consoleKeyboardButton->SetBorderColour(accent);
	mgr.AddField(consoleKeyboardButton);

	DisplayField::SetDefaultColours(text, pageBg);
#else
	DisplayField::SetDefaultColours(colours.buttonTextColour, colours.buttonImageBackColour);
	mgr.AddField(new IconButton(margin, DisplayX - margin - keyboardButtonWidth, keyboardButtonWidth, IconKeyboard, evKeyboard));
	DisplayField::SetDefaultColours(colours.labelTextColour, colours.defaultBackColour);
	mgr.AddField(new StaticTextField(margin + labelRowAdjust, margin, DisplayX - 2 * margin - keyboardButtonWidth, TextAlignment::Centre, strings->messages));
#endif

	PixelNumber row = firstMessageRow;
	for (unsigned int r = 0; r < numMessageRows; ++r)
	{
		StaticTextField *t = new StaticTextField(row, margin, messageTimeWidth, TextAlignment::Left, nullptr);
		mgr.AddField(t);
		messageTimeFields[r] = t;
		t = new StaticTextField(row, messageTextX, messageTextWidth, TextAlignment::Left, nullptr);
		mgr.AddField(t);
		messageTextFields[r] = t;
		row += rowTextHeight;
	}
	messageRoot = mgr.GetRoot();
}

// Create the fields for the Setup tab
static void CreateSetupTabFields(const ColourScheme& colours)
{
#if DISPLAY_X == 800
	UNUSED(colours);
	mgr.SetRoot(baseRoot);
	const Colour tile = UTFT::fromRGB(28, 34, 43);
	const Colour text = UTFT::fromRGB(229, 232, 236);
	const Colour muted = UTFT::fromRGB(154, 164, 178);
	const Colour neutralBorder = UTFT::fromRGB(59, 67, 79);
	const Colour accent = GetModernAccentColour();
	const Colour resetRed = UTFT::fromRGB(201, 50, 24);

	DisplayField::SetDefaultFont(glcd19x21);
	DisplayField::SetDefaultColours(muted, UTFT::fromRGB(18, 22, 28));
	mgr.AddField(new StaticTextField(101, 118, 37, TextAlignment::Left, "IP:"));
	mgr.AddField(new StaticTextField(101, 340, 100, TextAlignment::Left, "Feedrate:"));
	mgr.AddField(new StaticTextField(101, 545, 140, TextAlignment::Left, "Z Offset step:"));

	settingsIpText.copy(ipAddress.c_str());
	settingsFeedrateText.printf("%u", (unsigned int)nvData.GetFeedrate());
	settingsBabystepText.copy(babystepAmounts[nvData.GetBabystepAmountIndex()]);
	DisplayField::SetDefaultColours(text, tile);
	settingsIpValueButton = new ModernTextButton(88, 155, 160, 44, settingsIpText.c_str(), evNull, 0, glcd19x21, true);
	settingsIpValueButton->SetBorderColour(neutralBorder);
	settingsFeedrateValueButton = new ModernTextButton(88, 440, 90, 44, settingsFeedrateText.c_str(), evSetFeedrate, 0, glcd19x21, true);
	settingsFeedrateValueButton->SetBorderColour(neutralBorder);
	settingsBabystepValueButton = new ModernTextButton(88, 685, 90, 44, settingsBabystepText.c_str(), evSetBabystepAmount, 0, glcd19x21, true);
	settingsBabystepValueButton->SetBorderColour(neutralBorder);
	mgr.AddField(settingsIpValueButton);
	mgr.AddField(settingsFeedrateValueButton);
	mgr.AddField(settingsBabystepValueButton);

	struct SettingTileDef { PixelNumber x, y; const char *label; Event event; };
	static const SettingTileDef settingTiles[] = {
		{118,170,"VOLUME",             evSettingsVolumeOpen},
		{350,170,"BRIGHTNESS",         evSettingsBrightnessOpen},
		{582,170,"INFO TIMEOUT",       evSettingsInfoTimeoutOpen},
		{118,240,"ACCENT COLOR",       evSettingsAccentOpen},
		{350,240,"ALWAYS DIM",         evSettingsAlwaysDimToggle},
		{582,240,"BAUD",               evSettingsBaudOpen},
		{118,310,"TOUCH CALIBR.",      evSettingsTouchOpen},
		{350,310,"MIRROR DISPLAY",     evInvertX},
		{582,310,"INVERT DISPLAY",     evInvertY},
		{118,380,"HEAT NOT COMB",    evSettingsHeaterCombineOpen}
	};
	for (const SettingTileDef& def : settingTiles)
	{
		DisplayField::SetDefaultColours(text, tile);
		ModernTextButton * const b = new ModernTextButton(def.y, def.x, 200, 60, def.label, def.event, 0, glcd19x21, false);
		if (def.event == evSettingsAlwaysDimToggle)
		{
			settingsAlwaysDimButton = b;
		}
		else if (def.event == evSettingsHeaterCombineOpen)
		{
			settingsHeaterCombineButton = b;
		}
		mgr.AddField(b);
	}

	// Row 4 middle slot: read-only live free-RAM monitor.
	// Add the text field first, then the card, because AddField() prepends fields.
	// This makes the card render first and the RAM text render on top.
	DisplayField::SetDefaultColours(text, tile);
	freeMem = new IntegerField(399, 360, 180, TextAlignment::Centre, "RAM ", " B");
	freeMem->SetValue((int)GetFreeMemory());
	mgr.AddField(freeMem);

	mgr.AddField(new ModernCard(380, 350, 200, 60, tile, neutralBorder, false));

	DisplayField::SetDefaultColours(UTFT::fromRGB(245,245,245), resetRed);
	mgr.AddField(new ModernTextButton(380, 582, 200, 60, "FACTORY RESET", evSettingsFactoryResetOpen, 0, glcd19x21, false));

	if (settingsAlwaysDimButton != nullptr)
	{
		settingsAlwaysDimButton->SetBorderColour(accent);
	}
	if (settingsHeaterCombineButton != nullptr)
	{
		settingsHeaterCombineButton->SetBorderColour(accent);
	}
	setupRoot = mgr.GetRoot();
	DisplayField::SetDefaultFont(DEFAULT_FONT);
#else

	mgr.SetRoot(baseRoot);
	DisplayField::SetDefaultColours(colours.labelTextColour, colours.defaultBackColour);
	// The firmware version field doubles up as an area for displaying debug messages, so make it the full width of the display
	mgr.AddField(fwVersionField = new TextField(row1, margin, DisplayX, TextAlignment::Left, strings->firmwareVersion, VERSION_TEXT));
	mgr.AddField(freeMem = new IntegerField(row2, margin, DisplayX/2 - margin, TextAlignment::Left, "Free RAM: "));
	mgr.AddField(new ColourGradientField(ColourGradientTopPos, ColourGradientLeftPos, ColourGradientWidth, ColourGradientHeight));

	DisplayField::SetDefaultColours(colours.buttonTextColour, colours.buttonTextBackColour);
	baudRateButton = AddIntegerButton(row3, 0, 3, nullptr, " baud", evSetBaudRate);
	baudRateButton->SetValue(nvData.GetBaudRate());
	volumeButton = AddIntegerButton(row3, 1, 3, strings->volume, nullptr, evSetVolume);
	volumeButton->SetValue(nvData.GetVolume());
	AddTextButton(row4, 0, 3, strings->calibrateTouch, evCalTouch, nullptr);
	AddTextButton(row4, 1, 3, strings->mirrorDisplay, evInvertX, nullptr);
	AddTextButton(row4, 2, 3, strings->invertDisplay, evInvertY, nullptr);
	coloursButton = AddTextButton(row5, 0, 3, strings->colourSchemeNames[colours.index], evSetColours, nullptr);
	AddTextButton(row5, 1, 3, strings->brightnessDown, evDimmer, nullptr);
	AddTextButton(row5, 2, 3, strings->brightnessUp, evBrighter, nullptr);
	dimmingTypeButton = AddTextButton(row6, 0, 3, strings->displayDimmingNames[(unsigned int)nvData.GetDisplayDimmerType()], evSetDimmingType, nullptr);
	infoTimeoutButton = AddIntegerButton(row6, 1, 3, strings->infoTimeout, nullptr, evSetInfoTimeout);
	infoTimeoutButton->SetValue(infoTimeout);
	AddTextButton(row6, 2, 3, strings->clearSettings, evFactoryReset, nullptr);
	screensaverTimeoutButton = AddIntegerButton(row7, 0, 3, strings->screensaverAfter, nullptr, evSetScreensaverTimeout);
	screensaverTimeoutButton->SetValue(nvData.GetScreensaverTimeout() / 1000);

	const PixelNumber width = CalcWidth(3);
	mgr.AddField(babystepAmountButton = new TextButtonWithLabel(row7, CalcXPos(1, width), width, babystepAmounts[nvData.GetBabystepAmountIndex()], evSetBabystepAmount, nullptr, strings->babystepAmount));

	feedrateAmountButton = AddIntegerButton(row7, 2, 3, strings->feedrate, nullptr, evSetFeedrate);
	feedrateAmountButton->SetValue(nvData.GetFeedrate());

	heaterCombiningButton  = AddTextButton(row8, 0, 3, strings->heaterCombineTypeNames[(unsigned int)nvData.GetHeaterCombineType()], evSetHeaterCombineType, nullptr);
	logLevelButton = AddTextButton(row8, 1, 3, strings->logLevelNames[(unsigned int)MessageLog::LogLevelGet()], evSetLogLevel, nullptr);

	DisplayField::SetDefaultColours(colours.labelTextColour, colours.defaultBackColour);
	mgr.AddField(ipAddressField = new TextField(row9, margin, DisplayX/2 - margin, TextAlignment::Left, "IP: ", ipAddress.c_str()));
	setupRoot = mgr.GetRoot();

#endif
}

// Create the fields that are displayed on all pages
static void CreateCommonFields(const ColourScheme& colours)
{
	DisplayField::SetDefaultColours(colours.buttonTextColour, colours.buttonTextBackColour, colours.buttonBorderColour, colours.buttonGradColour,
									colours.buttonPressedBackColour, colours.buttonPressedGradColour, colours.pal);
	const Colour pageBg = UTFT::fromRGB(18, 22, 28);      // #12161c
	const Colour railBg = UTFT::fromRGB(18, 22, 28);       // #12161c, same as the page background so unselected rail tiles blend into it
	const Colour tile = UTFT::fromRGB(28, 34, 43);         // #1c222b
	const Colour text = UTFT::fromRGB(229, 232, 236);      // #e5e8ec
	const Colour accent = GetModernAccentColour();

	DisplayField::SetDefaultColours(text, tile, accent, tile, accent, accent, IconPaletteDark);
#if DISPLAY_X == 800
	// Permanent five-button left rail.  All five controls use the same x,
	// 76x76 footprint and 20 px vertical spacing.  Keep them as one group so
	// the legacy content relayout can never move STOP/ALERT into the page.
	railStopButton = new ModernStopButton(5, 7, 76, 76, evEmergencyStop, DEFAULT_FONT);
	tabControl = new ModernMasterNavButton(101, 7, 76, 76, MasterNavIcon::Joystick, evTabControl);
	tabStatus = new ModernMasterNavButton(197, 7, 76, 76, MasterNavIcon::List, evTabStatus);
	tabSystem = new ModernMasterNavButton(293, 7, 76, 76, MasterNavIcon::Gear, evTabSystem);
	railAlertButton = new ModernAlertNavButton(389, 7, 76, 76, evSystemConsole);
	mgr.AddField(railStopButton);
	mgr.AddField(tabControl);
	mgr.AddField(tabStatus);
	mgr.AddField(tabSystem);
	mgr.AddField(railAlertButton);

	// The backing card is part of the permanent rail too.  Keeping a pointer to
	// it is intentional: RelayoutLegacyFields() uses that identity to leave it
	// at x=0 instead of scaling/shifting it into the content pane.
	railBackground = new ModernCard(0, 0, masterTabWidth, DisplayY, railBg, railBg, false);
	mgr.AddField(railBackground);

	// Same rule for the shared Accent separator under the top tab row.
	topTabSeparator = new ModernCard(topTabHeight, topTabRowLeft, topTabRowWidth, 3, accent, accent, false);
	mgr.AddField(topTabSeparator);
#else
	// Preserve the compact-display rail geometry; the mock-up corner controls
	// are specific to the 800x480 modern UI.
	const PixelNumber masterWidth = masterTabWidth - 2 * margin;
	const PixelNumber masterHeight = 56;
	tabControl = new ModernMasterNavButton(margin, margin, masterWidth, masterHeight, MasterNavIcon::Joystick, evTabControl);
	tabStatus = new ModernMasterNavButton(DisplayY/3 - masterHeight/2, margin, masterWidth, masterHeight, MasterNavIcon::List, evTabStatus);
	tabSystem = new ModernMasterNavButton((2 * DisplayY)/3 - masterHeight/2, margin, masterWidth, masterHeight, MasterNavIcon::Gear, evTabSystem);
	mgr.AddField(tabControl);
	mgr.AddField(tabStatus);
	mgr.AddField(tabSystem);
	mgr.AddField(new ModernCard(0, 0, masterTabWidth, DisplayY, pageBg, pageBg, false));
#endif
}

static void AddControlSubTabs()
{
	mgr.SetRoot(controlRoot);
	AddTopTab(0, 4, "TOOLS", evControlTools);
	AddTopTab(1, 4, "MOVE", evControlMovement);
	AddTopTab(2, 4, "EXTRUDE", evControlExtrusion);
	AddTopTab(3, 4, "MACROS", evControlMacros);
	AddTopTabBackground();
	controlRoot = mgr.GetRoot();
}

static void AddStatusSubTabs(DisplayField *&root)
{
	mgr.SetRoot(root);
	AddTopTab(0, 4, "PRINTING", evStatusJobStatus);
	AddTopTab(1, 4, "TUNE", evStatusTune);
	AddTopTab(2, 4, "JOB", evStatusJob);
	AddTopTab(3, 4, "OBJECT", evStatusObjects);
	AddTopTabBackground();
	root = mgr.GetRoot();
}

static void AddSystemSubTabs(DisplayField *&root)
{
	mgr.SetRoot(root);
	AddTopTab(0, 2, "CONSOLE", evSystemConsole);
	AddTopTab(1, 2, "SETTINGS", evSystemSettings);
	AddTopTabBackground();
	root = mgr.GetRoot();
}


#if DISPLAY_X == 800
static PixelNumber ControlX(PixelNumber svgX)
{
	return contentLeft + static_cast<PixelNumber>((static_cast<uint32_t>(svgX - 90) * contentWidth) / 710);
}

static PixelNumber ControlW(PixelNumber svgW)
{
	return static_cast<PixelNumber>((static_cast<uint32_t>(svgW) * contentWidth) / 710);
}

static unsigned int CountControlToolResources()
{
	unsigned int count = 0;
	OM::IterateToolsWhile([&count](OM::Tool*&, size_t) { ++count; return true; });
	OM::IterateBedsWhile([&count](OM::Bed*& bed, size_t) {
		if (bed != nullptr && bed->heater >= 0) ++count;
		return true;
	});
	OM::IterateChambersWhile([&count](OM::Chamber*& chamber, size_t) {
		if (chamber != nullptr && chamber->heater >= 0) ++count;
		return true;
	});
	return count;
}

static bool GetControlToolResource(unsigned int wanted, ControlToolResource& result)
{
	result = ControlToolResource{};
	unsigned int pos = 0;
	bool found = false;
	OM::IterateToolsWhile([&](OM::Tool*& tool, size_t) {
		if (pos++ == wanted)
		{
			result.type = ControlToolResourceType::Tool;
			result.index = tool->index;
			result.heater = (tool->heaters[0] != nullptr) ? tool->heaters[0]->heaterIndex : -1;
			found = true;
			return false;
		}
		return true;
	});
	if (found) return true;
	OM::IterateBedsWhile([&](OM::Bed*& bed, size_t) {
		if (bed != nullptr && bed->heater >= 0)
		{
			if (pos++ == wanted)
			{
				result.type = ControlToolResourceType::Bed;
				result.index = bed->index;
				result.heater = bed->heater;
				found = true;
				return false;
			}
		}
		return true;
	});
	if (found) return true;
	OM::IterateChambersWhile([&](OM::Chamber*& chamber, size_t) {
		if (chamber != nullptr && chamber->heater >= 0)
		{
			if (pos++ == wanted)
			{
				result.type = ControlToolResourceType::Chamber;
				result.index = chamber->index;
				result.heater = chamber->heater;
				found = true;
				return false;
			}
		}
		return true;
	});
	return found;
}

static OM::HeaterStatus GetControlToolHeaterStatus(const ControlToolResource& resource)
{
	// Power/heat state must follow the actual RRF heater state for every
	// resource, including tools. Tool selection (active/standby tool) is a
	// separate concept and must not make the heater power tile appear on.
	if (resource.heater >= 0 && resource.heater < static_cast<int>(JobStatusMaxHeaters))
	{
		return jobStatusHeaterStatus[resource.heater];
	}
	return OM::HeaterStatus::off;
}

static int GetControlToolTarget(const ControlToolResource& resource, bool active)
{
	if (resource.type == ControlToolResourceType::Tool)
	{
		OM::Tool * const tool = OM::GetTool(resource.index);
		if (tool != nullptr && tool->heaters[0] != nullptr)
		{
			return active ? tool->heaters[0]->activeTemp : tool->heaters[0]->standbyTemp;
		}
	}
	if (resource.heater >= 0 && resource.heater < static_cast<int>(ControlToolMaxHeaters))
	{
		return active ? controlToolActiveTarget[resource.heater] : controlToolStandbyTarget[resource.heater];
	}
	return 0;
}

static void RefreshControlToolsPage()
{
	if (controlToolHeaderCards[0] == nullptr)
	{
		return;
	}

	const Colour tile = UTFT::fromRGB(28, 34, 43);
	const Colour text = UTFT::fromRGB(229, 232, 236);
	const Colour neutralBorder = UTFT::fromRGB(59, 67, 79);
	const Colour accent = GetModernAccentColour();
	const Colour activePower = UTFT::fromRGB(201, 50, 24);
	const Colour activePowerGlyph = UTFT::fromRGB(36, 36, 36);
	const Colour inactivePower = UTFT::fromRGB(42, 49, 60);
	const Colour inactivePowerGlyph = UTFT::fromRGB(138, 146, 160);
	const Colour heaterFault = UTFT::fromRGB(159, 62, 255);

	const unsigned int resourceCount = CountControlToolResources();
	const unsigned int perPage = (resourceCount > ControlToolVisibleColumns) ? ControlToolPagedColumns : ControlToolVisibleColumns;
	const unsigned int maxPage = (resourceCount == 0) ? 0 : (resourceCount - 1) / perPage;
	if (controlToolPage > maxPage) controlToolPage = maxPage;

	for (unsigned int column = 0; column < ControlToolVisibleColumns; ++column)
	{
		const bool columnAllowed = column < perPage;
		const unsigned int resourcePos = controlToolPage * perPage + column;
		ControlToolResource resource;
		const bool visible = columnAllowed && resourcePos < resourceCount && GetControlToolResource(resourcePos, resource);
		controlToolVisibleResource[column] = visible ? resource : ControlToolResource{};
		// The header's tap target must follow the column: left visible in an unused column it draws an empty tile
		// (its own fill) and reacts to touch. Show it first so that, when a column appears, the tile, name and
		// temperature are painted on top of it.
		if (controlToolHeaderButtons[column] != nullptr)
		{
			if (!visible)
			{
				controlToolHeaderButtons[column]->Press(false, 0);
			}
			mgr.Show(controlToolHeaderButtons[column], visible);
		}
		mgr.Show(controlToolHeaderCards[column], visible);
		mgr.Show(controlToolNameFields[column], visible);
		mgr.Show(controlToolCurrentFields[column], visible);
		mgr.Show(controlToolActiveButtons[column], visible);
		mgr.Show(controlToolStandbyButtons[column], visible);
		mgr.Show(controlToolPowerButtons[column], visible);
		if (!visible) continue;

		OM::HeaterStatus state = GetControlToolHeaterStatus(resource);
		const bool active = state == OM::HeaterStatus::active;
		const bool standby = state == OM::HeaterStatus::standby;
		const bool fault = state == OM::HeaterStatus::fault ||
			(resource.heater >= 0 && resource.heater < static_cast<int>(JobStatusMaxHeaters) &&
			 jobStatusHeaterStatus[resource.heater] == OM::HeaterStatus::fault);

		if (resource.type == ControlToolResourceType::Tool)
		{
			controlToolNameText[column].printf("T%d", resource.index);
		}
		else if (resource.type == ControlToolResourceType::Bed)
		{
			controlToolNameText[column].copy("BED");
		}
		else
		{
			// The final TOOLS mock-up uses the chamber pictogram instead of the
			// word CHAMBER, so leave the label empty and let ModernResourceLabel
			// centre the square/heat-wave glyph on its own.
			controlToolNameText[column].Clear();
		}
		controlToolNameFields[column]->SetText(controlToolNameText[column].c_str());
		controlToolNameFields[column]->SetIcon(
			resource.type == ControlToolResourceType::Bed ? ModernResourceIcon::Bed
			: resource.type == ControlToolResourceType::Chamber ? ModernResourceIcon::Chamber
			: ModernResourceIcon::None);

		if (resource.heater >= 0 && resource.heater < static_cast<int>(JobStatusMaxHeaters) && jobStatusHeaterValid[resource.heater])
		{
			controlToolCurrentText[column].printf("%.1f" DEGREE_SYMBOL "C", (double)jobStatusHeaterTemps[resource.heater]);
		}
		else
		{
			controlToolCurrentText[column].copy("---" DEGREE_SYMBOL "C");
		}
		controlToolCurrentFields[column]->SetValue(controlToolCurrentText[column].c_str());

		const int activeTarget = GetControlToolTarget(resource, true);
		const int standbyTarget = GetControlToolTarget(resource, false);
		controlToolActiveText[column].printf("%d", activeTarget);
		controlToolStandbyText[column].printf("%d", standbyTarget);
		controlToolActiveButtons[column]->SetText(controlToolActiveText[column].c_str());
		controlToolStandbyButtons[column]->SetText(controlToolStandbyText[column].c_str());
		controlToolActiveButtons[column]->SetEvent(evControlToolsActiveTemp, static_cast<int>(column));
		controlToolStandbyButtons[column]->SetEvent(evControlToolsStandbyTemp, static_cast<int>(column));

		const Colour headerFill = fault ? heaterFault : tile;
		const Colour headerText = fault ? text : (active ? accent : text);
		controlToolHeaderCards[column]->SetFillColour(headerFill);
		controlToolHeaderCards[column]->SetBorderVisible(active && !fault);
		controlToolHeaderCards[column]->SetBorderColour(accent);
		controlToolNameFields[column]->SetColours(headerText, headerFill);
		controlToolCurrentFields[column]->SetColours(headerText, headerFill);

		const Colour targetFill = fault ? heaterFault : tile;
		controlToolActiveButtons[column]->SetColours(fault ? text : (active ? accent : text), targetFill);
		controlToolActiveButtons[column]->SetBorderVisible(!fault && active);
		controlToolActiveButtons[column]->SetBorderColour(active ? accent : neutralBorder);
		controlToolStandbyButtons[column]->SetColours(fault ? text : (standby ? accent : text), targetFill);
		controlToolStandbyButtons[column]->SetBorderVisible(!fault && standby);
		controlToolStandbyButtons[column]->SetBorderColour(standby ? accent : neutralBorder);

		// Power tile reflects the heater itself, not whether a tool is selected.
		// Active, standby and tuning all mean the heater is on; only OFF is gray.
		// Fault keeps the dedicated purple override below.
		const bool powered = state != OM::HeaterStatus::off;
		controlToolPowerButtons[column]->SetColours(fault ? text : (powered ? activePowerGlyph : inactivePowerGlyph), fault ? heaterFault : (powered ? activePower : inactivePower));
		controlToolPowerButtons[column]->SetEvent((powered && !fault) ? evControlToolsPower : evNull, static_cast<int>(column));
		controlToolPowerButtons[column]->SetBorderVisible(false);
		controlToolPowerButtons[column]->SetChanged();
	}

	const bool paged = resourceCount > ControlToolVisibleColumns;
	mgr.Show(controlToolPageUpButton, paged && controlToolPage > 0);
	mgr.Show(controlToolPageDownButton, paged && controlToolPage < maxPage);
}

// Current heater temperatures arrive frequently from RRF. Do not rebuild the
// complete TOOLS page for every temperature sample; update only the visible
// temperature field that belongs to this heater. Full page refreshes are still
// used for structural/state changes such as page selection, tool status and
// heater status changes.
static void RefreshControlToolCurrentTemperature(size_t heaterIndex)
{
	if (heaterIndex >= JobStatusMaxHeaters)
	{
		return;
	}

	for (unsigned int column = 0; column < ControlToolVisibleColumns; ++column)
	{
		const ControlToolResource& resource = controlToolVisibleResource[column];
		if (resource.type == ControlToolResourceType::None ||
			resource.heater != static_cast<int>(heaterIndex) ||
			controlToolCurrentFields[column] == nullptr)
		{
			continue;
		}

		if (jobStatusHeaterValid[heaterIndex])
		{
			controlToolCurrentText[column].printf("%.1f" DEGREE_SYMBOL "C", (double)jobStatusHeaterTemps[heaterIndex]);
		}
		else
		{
			controlToolCurrentText[column].copy("---" DEGREE_SYMBOL "C");
		}
		controlToolCurrentFields[column]->SetValue(controlToolCurrentText[column].c_str());
	}
}

static void RefreshControlToolCurrentTemperatures()
{
	for (unsigned int column = 0; column < ControlToolVisibleColumns; ++column)
	{
		const ControlToolResource& resource = controlToolVisibleResource[column];
		if (resource.type != ControlToolResourceType::None && resource.heater >= 0)
		{
			RefreshControlToolCurrentTemperature(static_cast<size_t>(resource.heater));
		}
	}
}

static void ConfigureNumericPopupForTemperature()
{
	numericPopupContext = NumericPopupContext::Temperature;
	const Colour tile = UTFT::fromRGB(28, 34, 43);
	const Colour dimText = UTFT::fromRGB(154, 164, 178);
	if (controlTempNumpadUnitField != nullptr)
	{
		controlTempNumpadUnitField->Show(true);
	}
	if (controlTempNumpadResourceField != nullptr)
	{
		controlTempNumpadResourceField->SetPosition(415, 29);
		controlTempNumpadResourceField->SetPositionAndWidth(415, 120);
	}
	if (controlTempNumpadDecimalButton != nullptr)
	{
		controlTempNumpadDecimalButton->SetColours(dimText, tile);
		controlTempNumpadDecimalButton->SetEvent(evNull, 0);
	}
	if (controlTempNumpadMinusButton != nullptr)
	{
		controlTempNumpadMinusButton->SetColours(dimText, tile);
		controlTempNumpadMinusButton->SetEvent(evNull, 0);
	}
}

static void RefreshControlTempNumpadValue()
{
	controlTempNumpadValueText.printf("%u", controlTempNumpadValue);
	if (controlTempNumpadValueField != nullptr)
	{
		controlTempNumpadValueField->SetValue(controlTempNumpadValueText.c_str());
	}
}

static void OpenControlTempNumpad(unsigned int column, bool activeTarget)
{
	if (column >= ControlToolVisibleColumns || controlToolVisibleResource[column].type == ControlToolResourceType::None)
	{
		return;
	}
	ConfigureNumericPopupForTemperature();
	controlTempNumpadResource = controlToolVisibleResource[column];
	controlTempNumpadActiveTarget = activeTarget;
	int val = GetControlToolTarget(controlTempNumpadResource, activeTarget);
	if (val < 0) val = 0;
	if (val > 999) val = 999;
	controlTempNumpadValue = static_cast<unsigned int>(val);
	controlTempNumpadFresh = true;
	if (controlTempNumpadResource.type == ControlToolResourceType::Tool)
	{
		controlTempNumpadResourceText.printf("T%d", controlTempNumpadResource.index);
	}
	else if (controlTempNumpadResource.type == ControlToolResourceType::Bed)
	{
		controlTempNumpadResourceText.copy("BED");
	}
	else
	{
		controlTempNumpadResourceText.copy("CHAMBER");
	}
	controlTempNumpadResourceField->SetText(controlTempNumpadResourceText.c_str());
	RefreshControlTempNumpadValue();
	mgr.SetPopup(controlTempNumpadPopup, AutoPlace, AutoPlace);
}

static void ShowModernAlert(const char *message);

static bool ValidateControlTemperatureTarget(int value, int& minValue, int& maxValue)
{
	switch (controlTempNumpadResource.type)
	{
	case ControlToolResourceType::Tool:
		minValue = ExtruderMinTemp;
		maxValue = ExtruderMaxTemp;
		break;

	case ControlToolResourceType::Bed:
		minValue = BedMinTemp;
		maxValue = BedMaxTemp;
		break;

	case ControlToolResourceType::Chamber:
		minValue = ChamberMinTemp;
		maxValue = ChamberMaxTemp;
		break;

	default:
		return false;
	}

	return value >= minValue && value <= maxValue;
}

static void ShowControlTemperatureRangeAlert(int minValue, int maxValue)
{
	String<64> message;
	message.printf("Temperature range %d-%d C", minValue, maxValue);
	ShowModernAlert(message.c_str());
}

static void SendControlTemperatureTarget()
{
	const int value = static_cast<int>(controlTempNumpadValue);
	const bool active = controlTempNumpadActiveTarget;
	const ControlToolResource resource = controlTempNumpadResource;
	if (resource.type == ControlToolResourceType::Tool)
	{
		OM::Tool * const tool = OM::GetTool(resource.index);
		if (tool == nullptr || tool->heaters[0] == nullptr) return;
		const bool useM568 = GetFirmwareFeatures().IsBitSet(m568TempAndRPM);
		if (nvData.GetHeaterCombineType() == HeaterCombineType::combined)
		{
			SerialIo::Sendf("%s P%d %c%d\n", useM568 ? "M568" : "G10", resource.index, active ? 'S' : 'R', value);
		}
		else
		{
			const int old = active ? tool->heaters[0]->activeTemp : tool->heaters[0]->standbyTemp;
			tool->UpdateTemp(0, value, active);
			String<maxUserCommandLength> temps;
			if (tool->GetHeaterTemps(temps.GetRef(), active))
			{
				SerialIo::Sendf("%s P%d %c%s\n", useM568 ? "M568" : "G10", resource.index, active ? 'S' : 'R', temps.c_str());
			}
			tool->UpdateTemp(0, old, active); // wait for RRF to authoritatively report the new target
		}
	}
	else if (resource.type == ControlToolResourceType::Bed)
	{
		SerialIo::Sendf("M140 P%d %c%d\n", resource.index, active ? 'S' : 'R', value);
	}
	else if (resource.type == ControlToolResourceType::Chamber)
	{
		SerialIo::Sendf("M141 P%d %c%d\n", resource.index, active ? 'S' : 'R', value);
	}
}

static void OpenControlToolChange(int targetTool)
{
	const OM::PrinterStatus printerState = GetStatus();
	if (printerState == OM::PrinterStatus::printing || printerState == OM::PrinterStatus::simulating)
	{
		return;
	}
	controlToolChangeTarget = (targetTool == currentTool) ? NoTool : targetTool;
	if (currentTool >= 0) controlToolChangeFromText.printf("T%d", currentTool);
	else controlToolChangeFromText.copy("OFF");
	if (controlToolChangeTarget >= 0) controlToolChangeToText.printf("T%d", controlToolChangeTarget);
	else controlToolChangeToText.copy("OFF");
	controlToolChangeSummaryText.printf("%s   >>>   %s", controlToolChangeFromText.c_str(), controlToolChangeToText.c_str());
	standardPopupContext = StandardPopupContext::ControlToolChange;
	ConfigureStandardPopupTitle("ALERT !", false);
	ResetStandardPopupContent();
	ConfigureStandardPopupInformation("This operation requires", "tool change:", controlToolChangeSummaryText.c_str());
	standardPopupCancelButton->Show(true);
	standardPopupConfirmButton->Show(true);
	mgr.SetPopup(standardPopup, AutoPlace, AutoPlace);
}

// Confirmation gate for turning an already-on heater off (Bed/Chamber; Tool
// power taps go through OpenControlToolChange instead, unchanged). Stores
// which resource to act on so the Confirm button knows what to send.
static void OpenControlHeaterOff(const ControlToolResource& resource)
{
	controlHeaterOffResource = resource;
	if (resource.type == ControlToolResourceType::Tool)
	{
		controlHeaterOffMessageText.printf("Turn the T%d heater OFF?", resource.index);
	}
	else if (resource.type == ControlToolResourceType::Bed)
	{
		controlHeaterOffMessageText.copy("Turn the BED heater OFF?");
	}
	else if (resource.type == ControlToolResourceType::Chamber)
	{
		controlHeaterOffMessageText.copy("Turn the CHAMBER heater OFF?");
	}
	else
	{
		return;
	}
	standardPopupContext = StandardPopupContext::ControlHeaterOff;
	ConfigureStandardPopupTitle("ALERT !", false);
	ResetStandardPopupContent();
	ConfigureStandardPopupInformation(controlHeaterOffMessageText.c_str());
	standardPopupCancelButton->Show(true);
	standardPopupConfirmButton->Show(true);
	mgr.SetPopup(standardPopup, AutoPlace, AutoPlace);
}

static void HandleControlHeaterOffConfirm()
{
	const ControlToolResource resource = controlHeaterOffResource;
	// Every variant sets BOTH the active (S) and the standby (R) temperature to 0, so the heater cannot heat up
	// again when the tool/bed/chamber later changes between its active and standby states.
	if (resource.type == ControlToolResourceType::Tool)
	{
		// Tool heaters are switched off through the tool (M568/G10 P<tool>), which works whether or not that tool is
		// the selected one: S0 covers the active state and R0 the standby state. A tool with several heaters gets
		// one 0 per heater (0:0:...) so that every one of its heaters is switched off, not just the first.
		const bool useM568 = GetFirmwareFeatures().IsBitSet(m568TempAndRPM);
		String<2 * MaxHeatersPerTool> zeros;
		const OM::Tool * const tool = OM::GetTool(resource.index);
		size_t numHeaters = 0;
		if (tool != nullptr)
		{
			while (numHeaters < MaxHeatersPerTool && tool->heaters[numHeaters] != nullptr)
			{
				++numHeaters;
			}
		}
		zeros.copy("0");
		for (size_t i = 1; i < numHeaters; ++i)
		{
			zeros.cat(":0");
		}
		SerialIo::Sendf("%s P%d S%s R%s\n", useM568 ? "M568" : "G10", resource.index, zeros.c_str(), zeros.c_str());
	}
	else if (resource.type == ControlToolResourceType::Bed)
	{
		// Address the heater directly with H, together with the bed index P so the command can never be taken
		// for bed 0. The heater number comes from RRF's own object model, so this does not remap anything.
		const OM::Bed * const bed = OM::GetBed(resource.index);
		if (bed != nullptr && bed->heater >= 0)
		{
			SerialIo::Sendf("M140 P%d H%d S0 R0\n", resource.index, static_cast<int>(bed->heater));
		}
		else
		{
			SerialIo::Sendf("M140 P%d S0 R0\n", resource.index);
		}
	}
	else if (resource.type == ControlToolResourceType::Chamber)
	{
		const OM::Chamber * const chamber = OM::GetChamber(resource.index);
		if (chamber != nullptr && chamber->heater >= 0)
		{
			SerialIo::Sendf("M141 P%d H%d S0 R0\n", resource.index, static_cast<int>(chamber->heater));
		}
		else
		{
			SerialIo::Sendf("M141 P%d S0 R0\n", resource.index);
		}
	}
}

// Tapping the T#/BED/CHAMBER header tile itself. The chamber header is informational only (RRF has no G-code to put a
// chamber heater into standby, as far as I could find) and this is a no-op for it.
//  - The tool that is currently selected: just flip its heaters between the active and the standby temperature.
//    Nothing is parked or deselected, so no tool change is involved.
//  - Any other tool: ask to change to that tool.
static void HandleControlToolHeaderTap(unsigned int column)
{
	if (column >= ControlToolVisibleColumns) return;
	const ControlToolResource resource = controlToolVisibleResource[column];

	if (resource.type == ControlToolResourceType::Bed)
	{
		// Bed heaters also have an active and a standby state: M144 switches the bed to its standby temperature and
		// M140 without a temperature switches it back to its active one. Only meaningful while the heater is on.
		const OM::PrinterStatus printerState = GetStatus();
		if (printerState == OM::PrinterStatus::printing || printerState == OM::PrinterStatus::simulating)
		{
			return;
		}
		const OM::HeaterStatus heaterState = GetControlToolHeaterStatus(resource);
		if (heaterState == OM::HeaterStatus::active)
		{
			SerialIo::Sendf("M144 P%d\n", resource.index);
		}
		else if (heaterState == OM::HeaterStatus::standby)
		{
			SerialIo::Sendf("M140 P%d\n", resource.index);
		}
		return;
	}

	if (resource.type != ControlToolResourceType::Tool) return;

	if (resource.index == currentTool)
	{
		const OM::PrinterStatus printerState = GetStatus();
		if (printerState == OM::PrinterStatus::printing || printerState == OM::PrinterStatus::simulating)
		{
			return;			// don't change heater states under a running job
		}
		const OM::Tool * const tool = OM::GetTool(resource.index);
		if (tool != nullptr && GetFirmwareFeatures().IsBitSet(m568TempAndRPM))
		{
			// M568 A: 0 = off, 1 = standby, 2 = active. It changes the state of the tool's heaters without selecting or deselecting the tool.
			SerialIo::Sendf("M568 P%d A%d\n", resource.index, (tool->status == OM::ToolStatus::active) ? 1 : 2);
		}
		return;
	}
	OpenControlToolChange(resource.index);
}

static void HandleControlToolPower(unsigned int column)
{
	if (column >= ControlToolVisibleColumns) return;
	const ControlToolResource resource = controlToolVisibleResource[column];
	// The power button only ever turns a heater OFF (via confirmation).
	// Turning a heater on is exclusively done by tapping the active or
	// standby temperature tile, which opens the numpad to pick a target.
	// Permit OFF from any real powered state (active/standby/tuning), while
	// ignoring already-off and faulted heaters.
	const OM::HeaterStatus heaterState = GetControlToolHeaterStatus(resource);
	if (heaterState == OM::HeaterStatus::off || heaterState == OM::HeaterStatus::fault) return;
	if (resource.type == ControlToolResourceType::Tool)
	{
		const OM::Tool * const tool = OM::GetTool(resource.index);
		if (tool == nullptr) return;
		OpenControlHeaterOff(resource);
	}
	else if (resource.type == ControlToolResourceType::Bed)
	{
		const OM::Bed * const bed = OM::GetBed(resource.index);
		if (bed == nullptr) return;
		OpenControlHeaterOff(resource);
	}
	else if (resource.type == ControlToolResourceType::Chamber)
	{
		const OM::Chamber * const chamber = OM::GetChamber(resource.index);
		if (chamber == nullptr) return;
		OpenControlHeaterOff(resource);
	}
}

static void CreateControlToolsPopups(const ColourScheme& colours)
{
	const Colour pageBg = UTFT::fromRGB(18, 22, 28);
	const Colour tile = UTFT::fromRGB(28, 34, 43);
	const Colour text = UTFT::fromRGB(229, 232, 236);
	const Colour neutralBorder = UTFT::fromRGB(59, 67, 79);
	const Colour accent = GetModernAccentColour();
	const Colour cancelRed = UTFT::fromRGB(201, 50, 24);
	const Colour confirmGreen = UTFT::fromRGB(86, 184, 52);   // check mark fill #56B834

	// Shared modern numeric keypad. Every tile (digits, ./-, backspace,
	// cancel, confirm) is now 110x78 -- the same size as the standard
	// Trash/X/tick action buttons -- instead of the original 95x58, with a
	// consistent 10px horizontal / 6px vertical gap between tiles. The whole
	// block (470x330) is centred in the 660x460 frame: 95px left/right
	// margin, and a 29px top margin above the value row / 29px below the
	// bottom row (52+20+330=402 tall, (460-402)/2=29).
	controlTempNumpadPopup = new PopupWindow(460, 660, pageBg, pageBg);
	DisplayField::SetDefaultFont(glcd28x32);
	DisplayField::SetDefaultColours(text, tile);
	controlTempNumpadValueField = new StaticTextField(39, 141, 185, TextAlignment::Left, "0");
	controlTempNumpadPopup->AddField(controlTempNumpadValueField);
	controlTempNumpadUnitField = new StaticTextField(39, 350, 40, TextAlignment::Right, DEGREE_SYMBOL "C");
	controlTempNumpadPopup->AddField(controlTempNumpadUnitField);
	controlTempNumpadPopup->AddField(new ModernCard(29, 125, 280, 52, tile, neutralBorder, true));
	controlTempNumpadResourceField = new ModernTextButton(29, 415, 120, 52, "T0", evNull, 0, glcd28x32);
	controlTempNumpadPopup->AddField(controlTempNumpadResourceField);

	static const char * const digitLabels[10] = { "0", "1", "2", "3", "4", "5", "6", "7", "8", "9" };
	for (unsigned int d = 1; d <= 9; ++d)
	{
		const unsigned int i = d - 1;
		const PixelNumber x = 95 + (i % 3) * 120;
		const PixelNumber y = 101 + (i / 3) * 84;
		controlTempNumpadPopup->AddField(new ModernTextButton(y, x, 110, 78, digitLabels[d], evNumericKey, static_cast<int>(d), glcd28x32));
	}

	// Bottom row follows v10 exactly: decimal / zero / minus. Decimal and minus
	// are deliberately non-interactive for heater temperatures.
	const Colour dimText = UTFT::fromRGB(154, 164, 178);
	DisplayField::SetDefaultColours(dimText, tile);
	controlTempNumpadDecimalButton = new ModernTextButton(353, 95, 110, 78, ".", evNull, 0, glcd28x32);
	controlTempNumpadPopup->AddField(controlTempNumpadDecimalButton);
	DisplayField::SetDefaultColours(text, tile);
	controlTempNumpadPopup->AddField(new ModernTextButton(353, 215, 110, 78, digitLabels[0], evNumericKey, 0, glcd28x32));
	DisplayField::SetDefaultColours(dimText, tile);
	controlTempNumpadMinusButton = new ModernTextButton(353, 335, 110, 78, "-", evNull, 0, glcd28x32);
	controlTempNumpadPopup->AddField(controlTempNumpadMinusButton);

	// Side controls match the v10 SVG. Backspace is neutral, X is semantic red,
	// and the check mark is semantic green.
	DisplayField::SetDefaultColours(dimText, tile);
	controlTempNumpadPopup->AddField(new ModernIconButton(101, 455, 110, 78, IconBackspace, evNumericBack));
	DisplayField::SetDefaultColours(UTFT::fromRGB(36, 36, 36), cancelRed);
	controlTempNumpadPopup->AddField(new ModernIconButton(185, 455, 110, 78, IconCancel, evNumericCancel));
	DisplayField::SetDefaultColours(UTFT::fromRGB(36, 36, 36), confirmGreen);
	controlTempNumpadPopup->AddField(new ModernIconButton(269, 455, 110, 78, IconOk, evNumericOk));

	// Build the SVG's 4 px inset Accent frame, now spanning the full 660x460
	// window instead of the old 450x362 one. Add inner first because AddField
	// prepends, making the outer frame render first and the controls render last.
	controlTempNumpadPopup->AddField(new ModernCard(8, 8, 644, 444, pageBg, accent, true));
	controlTempNumpadPopup->AddField(new ModernCard(6, 6, 648, 448, pageBg, accent, true));


	DisplayField::SetDefaultFont(DEFAULT_FONT);
	UNUSED(colours);
}

static void CreateControlToolsTabFields(const ColourScheme& colours)
{
	mgr.SetRoot(baseRoot);
	const Colour pageBg = UTFT::fromRGB(18, 22, 28);
	const Colour tile = UTFT::fromRGB(28, 34, 43);
	const Colour text = UTFT::fromRGB(229, 232, 236);
	const Colour muted = UTFT::fromRGB(154, 164, 178);
	const Colour neutralBorder = UTFT::fromRGB(59, 67, 79);

	DisplayField::SetDefaultFont(glcd28x32);
	for (unsigned int column = 0; column < ControlToolVisibleColumns; ++column)
	{
		const PixelNumber x = ControlX(98 + column * 140);
		const PixelNumber w = ControlW(126);

		DisplayField::SetDefaultColours(text, tile);
		controlToolNameFields[column] = new ModernResourceLabel(96, x, w, 34, "", glcd28x32);
		controlToolCurrentFields[column] = new StaticTextField(149, x, w, TextAlignment::Centre, "---" DEGREE_SYMBOL "C");
		mgr.AddField(controlToolNameFields[column]);
		mgr.AddField(controlToolCurrentFields[column]);
		controlToolHeaderCards[column] = new ModernCard(84, x, w, 114, tile, neutralBorder, false);
		mgr.AddField(controlToolHeaderCards[column]);
		// Invisible touch target over the header tile so tapping T#/BED/CHAMBER
		// opens the tool-change confirmation. Same fill as the tile so nothing
		// changes visually; it sits behind the name/current-temp text in the
		// paint order (added after them here, which the linked-list renderer
		// draws first/behind, per the AddField-prepends convention used
		// elsewhere in this function).
		controlToolHeaderButtons[column] = new ModernTouchArea(84, x, w, 114, evControlToolsHeaderTap, static_cast<int>(column));
		mgr.AddField(controlToolHeaderButtons[column]);

		controlToolActiveText[column].copy("0");
		controlToolStandbyText[column].copy("0");
		controlToolActiveButtons[column] = new ModernTemperatureButton(206, x, w, 89, controlToolActiveText[column].c_str(),
			ModernTemperatureIcon::Active, evControlToolsActiveTemp, static_cast<int>(column), DEFAULT_FONT);
		controlToolStandbyButtons[column] = new ModernTemperatureButton(303, x, w, 89, controlToolStandbyText[column].c_str(),
			ModernTemperatureIcon::Standby, evControlToolsStandbyTemp, static_cast<int>(column), DEFAULT_FONT);
		mgr.AddField(controlToolActiveButtons[column]);
		mgr.AddField(controlToolStandbyButtons[column]);

		DisplayField::SetDefaultColours(muted, UTFT::fromRGB(42, 49, 60));
		controlToolPowerButtons[column] = new ModernPowerButton(400, x, w, 56, evControlToolsPower, static_cast<int>(column));
		mgr.AddField(controlToolPowerButtons[column]);
	}

	DisplayField::SetDefaultColours(text, tile);
	controlToolPageUpButton = new ModernIconButton(188, ControlX(736), ControlW(54), 118, IconUp, evControlToolsPageUp);
	controlToolPageDownButton = new ModernIconButton(312, ControlX(736), ControlW(54), 118, IconDown, evControlToolsPageDown);
	mgr.AddField(controlToolPageUpButton);
	mgr.AddField(controlToolPageDownButton);

	controlToolsRoot = mgr.GetRoot();
	// Add the four CONTROL sub-tabs to the modern TOOLS root.
	mgr.SetRoot(controlToolsRoot);
	AddTopTab(0, 4, "TOOLS", evControlTools);
	AddTopTab(1, 4, "MOVE", evControlMovement);
	AddTopTab(2, 4, "EXTRUDE", evControlExtrusion);
	AddTopTab(3, 4, "MACROS", evControlMacros);
	AddTopTabBackground();
	controlToolsRoot = mgr.GetRoot();

	// Background is intentionally added last so it is painted first by the linked-list renderer.
	mgr.SetRoot(controlToolsRoot);
	mgr.AddField(new ModernCard(0, masterTabWidth, DisplayX - masterTabWidth, DisplayY, pageBg, pageBg));
	controlToolsRoot = mgr.GetRoot();
	DisplayField::SetDefaultFont(DEFAULT_FONT);
	CreateControlToolsPopups(colours);
	RefreshControlToolsPage();
}

static OM::Axis *GetControlMoveAxis(unsigned int axisSlot)
{
	if (axisSlot >= ControlMoveAxisCount)
	{
		return nullptr;
	}
	const char wanted = "XYZ"[axisSlot];
	OM::Axis *result = nullptr;
	OM::IterateAxesWhile([&](OM::Axis*& axis, size_t) {
		if (axis != nullptr && axis->visible && toupper(axis->letter[0]) == wanted)
		{
			result = axis;
			return false;
		}
		return true;
	});
	return result;
}

static int GetControlMoveAxisSlot(char letter)
{
	switch (toupper(letter))
	{
	case 'X': return 0;
	case 'Y': return 1;
	case 'Z': return 2;
	default: return -1;
	}
}

static void RefreshControlMovePosition(unsigned int axisSlot)
{
	if (axisSlot >= ControlMoveAxisCount || controlMovePositionFields[axisSlot] == nullptr)
	{
		return;
	}
	if (!controlMovePositionValid[axisSlot])
	{
		controlMovePositionText[axisSlot].copy("---");
	}
	else
	{
		const float value = controlMovePosition[axisSlot];
		const int scaled100 = static_cast<int>(value * 100.0f + ((value >= 0.0f) ? 0.5f : -0.5f));
		const unsigned int magnitude = static_cast<unsigned int>((scaled100 < 0) ? -scaled100 : scaled100);
		if ((magnitude % 100u) == 0u)
		{
			controlMovePositionText[axisSlot].printf("%.0f", (double)value);
		}
		else if ((magnitude % 10u) == 0u)
		{
			controlMovePositionText[axisSlot].printf("%.1f", (double)value);
		}
		else
		{
			controlMovePositionText[axisSlot].printf("%.2f", (double)value);
		}
	}
	controlMovePositionFields[axisSlot]->SetValue(controlMovePositionText[axisSlot].c_str());
}

static void RefreshControlMoveSteps()
{
	const Colour tile = UTFT::fromRGB(28, 34, 43);
	const Colour text = UTFT::fromRGB(229, 232, 236);
	const Colour accent = GetModernAccentColour();
	for (unsigned int i = 0; i < ControlMoveStepCount; ++i)
	{
		if (controlMoveStepButtons[i] == nullptr) continue;
		const bool selected = (i == controlMoveSelectedStep);
		controlMoveStepButtons[i]->SetColours(selected ? accent : text, tile);
		controlMoveStepButtons[i]->SetBorderVisible(selected);
		controlMoveStepButtons[i]->SetBorderColour(accent);
	}
}

static void RefreshControlMoveHoming()
{
	const Colour tile = UTFT::fromRGB(28, 34, 43);
	const Colour normal = UTFT::fromRGB(188, 196, 207);
	const Colour accent = GetModernAccentColour();
	for (unsigned int axisSlot = 0; axisSlot < ControlMoveAxisCount; ++axisSlot)
	{
		if (controlMoveHomeButtons[axisSlot] == nullptr) continue;
		const OM::Axis * const axis = GetControlMoveAxis(axisSlot);
		const bool exists = axis != nullptr;
		const bool homed = exists && axis->homed;
		controlMoveHomeButtons[axisSlot]->SetEvent(exists ? evControlMoveHome : evNull, static_cast<int>(axisSlot));
		controlMoveHomeButtons[axisSlot]->SetColours(homed ? accent : normal, tile);
		controlMoveHomeButtons[axisSlot]->SetBorderVisible(homed);
		controlMoveHomeButtons[axisSlot]->SetBorderColour(accent);
	}

	if (controlMoveHomeAllButton != nullptr)
	{
		bool anyVisible = false;
		bool allHomed = true;
		OM::IterateAxesWhile([&](OM::Axis*& axis, size_t) {
			if (axis != nullptr && axis->visible)
			{
				anyVisible = true;
				if (!axis->homed)
				{
					allHomed = false;
					return false;
				}
			}
			return true;
		});
		allHomed = anyVisible && allHomed;
		controlMoveHomeAllButton->SetColours(allHomed ? accent : normal, tile);
		controlMoveHomeAllButton->SetBorderVisible(allHomed);
		controlMoveHomeAllButton->SetBorderColour(accent);
	}
}

static void ShowModernAlert(const char *message)
{
	if (standardPopup == nullptr)
	{
		return;
	}
	modernAlertMessageText.copy(message != nullptr ? message : "ALERT");
	standardPopupContext = StandardPopupContext::ModernAlert;
	ConfigureStandardPopupTitle("ALERT !", false);
	ResetStandardPopupContent();
	ConfigureStandardPopupInformation(modernAlertMessageText.c_str());
	standardPopupCancelButton->SetPosition(275, 332);
	standardPopupCancelButton->Show(true);
	standardPopupConfirmButton->Show(false);
	mgr.SetPopup(standardPopup, AutoPlace, AutoPlace);
}

// Modern UI safety gate for pages that should not be entered while a print job
// owns the machine. A paused job still counts as active for MOVE, MACROS and JOB,
// but EXTRUDE deliberately passes allowPaused=true so filament actions remain
// available while the print is paused.
static bool IsModernPageLockedByPrint(bool allowPaused)
{
	const OM::PrinterStatus status = GetStatus();
	if (status == OM::PrinterStatus::printing ||
		status == OM::PrinterStatus::pausing ||
		status == OM::PrinterStatus::resuming ||
		status == OM::PrinterStatus::simulating)
	{
		return true;
	}
	return !allowPaused && status == OM::PrinterStatus::paused;
}

static void SetModernInfoPopupText(const char *title, const char *text, bool isError)
{
	const char *remaining = (text != nullptr) ? text : "";
	for (unsigned int line = 0; line < 4; ++line)
	{
		if (*remaining == '\0')
		{
			modernInfoText[line].Clear();
		}
		else if (line < 3)
		{
			const size_t splitPoint = MessageLog::FindSplitPoint(remaining, modernInfoText[line].Capacity(), 500);
			modernInfoText[line].copy(remaining);
			modernInfoText[line].Truncate(splitPoint);
			remaining += splitPoint;
			while (*remaining == ' ' || *remaining == '\r' || *remaining == '\n' || *remaining == '\t')
			{
				++remaining;
			}
		}
		else
		{
			modernInfoText[line].copy(remaining);
		}
	}

	standardPopupContext = StandardPopupContext::ModernInfo;
	ConfigureStandardPopupTitle((title != nullptr && *title != '\0') ? title : (isError ? "ALERT !" : ""), false);
	ResetStandardPopupContent();
	if (!isError && (title == nullptr || *title == '\0'))
	{
		// Ordinary information/response popups intentionally have no title tile.
		standardPopupNameCard->Show(false);
		standardPopupNameField->Show(false);
	}

	if (modernInfoText[3].strlen() != 0)
	{
		ConfigureStandardPopupInformation(modernInfoText[0].c_str(), modernInfoText[1].c_str(), modernInfoText[2].c_str(), modernInfoText[3].c_str());
	}
	else if (modernInfoText[2].strlen() != 0)
	{
		ConfigureStandardPopupInformation(modernInfoText[0].c_str(), modernInfoText[1].c_str(), modernInfoText[2].c_str());
	}
	else if (modernInfoText[1].strlen() != 0)
	{
		ConfigureStandardPopupInformation(modernInfoText[0].c_str(), modernInfoText[1].c_str());
	}
	else
	{
		ConfigureStandardPopupInformation(modernInfoText[0].c_str());
	}

	standardPopupCancelButton->SetPosition(275, 332);
	standardPopupCancelButton->Show(true);
	standardPopupConfirmButton->Show(false);
}

static void ClearDisplayedModernInfoPopup()
{
	if (displayingModernInfoPopup && standardPopup != nullptr &&
		standardPopupContext == StandardPopupContext::ModernInfo && mgr.IsPopupActive(standardPopup))
	{
		mgr.ClearPopup(true, standardPopup);
		standardPopupContext = StandardPopupContext::None;
		standardPopupChoiceCount = 0;
	}
	displayingModernInfoPopup = false;
}

static void ShowModernInfoPopup(const char *title, const char *text, bool isError)
{
	if (standardPopup == nullptr)
	{
		return;
	}

	// Do not overwrite an interactive shared popup. The response remains available
	// in the console/message log and can be shown on the next response instead.
	const bool standardAlreadyActive = mgr.IsPopupActive(standardPopup);
	if (standardAlreadyActive && standardPopupContext != StandardPopupContext::ModernInfo)
	{
		return;
	}

	// A normal response is allowed to replace an older non-modal legacy alert.
	if (alertPopup != nullptr && mgr.IsPopupActive(alertPopup))
	{
		mgr.ClearPopup(true, alertPopup);
	}

	SetModernInfoPopupText(title, text, isError);
	if (standardAlreadyActive)
	{
		mgr.Refresh(true);
	}
	else
	{
		mgr.SetPopup(standardPopup, AutoPlace, AutoPlace);
	}
	displayingModernInfoPopup = true;
}

static bool IsSharedM291AlertContext()
{
	return standardPopupContext == StandardPopupContext::M291Confirm ||
		standardPopupContext == StandardPopupContext::M291ConfirmControls ||
		standardPopupContext == StandardPopupContext::M291Choices ||
		standardPopupContext == StandardPopupContext::M291NumberInt ||
		standardPopupContext == StandardPopupContext::M291NumberFloat ||
		standardPopupContext == StandardPopupContext::M291Text;
}

static bool ClearSharedM291AlertPopup()
{
	if (standardPopup != nullptr && IsSharedM291AlertContext() && mgr.IsPopupActive(standardPopup))
	{
		if (numericPopupContext != NumericPopupContext::Temperature &&
			controlTempNumpadPopup != nullptr && mgr.IsPopupActive(controlTempNumpadPopup))
		{
			mgr.ClearPopup(true, controlTempNumpadPopup);
			numericPopupContext = NumericPopupContext::Temperature;
		}
		if (standardPopupContext == StandardPopupContext::M291Text && keyboardPopup != nullptr && mgr.IsPopupActive(keyboardPopup))
		{
			mgr.ClearPopup(true, keyboardPopup);
			keyboardIsDisplayed = false;
			keyboardDataHandler = SendGcode;
		}
		mgr.ClearPopup(true, standardPopup);
		standardPopupContext = StandardPopupContext::None;
		standardPopupChoiceCount = 0;
		standardPopupM291Seq = 0;
		return true;
	}
	return false;
}

static void PrepareStandardPopupForIncomingM291()
{
	// RRF M291 is printer-modal and takes priority over local UI confirmation
	// popups. Dismiss any local shared popup without committing its pending value.
	ClearDisplayedModernInfoPopup();
	ClearSharedM291AlertPopup();
	if (standardPopup != nullptr && mgr.IsPopupActive(standardPopup))
	{
		mgr.ClearPopup(true, standardPopup);
		standardPopupContext = StandardPopupContext::None;
		standardPopupChoiceCount = 0;
	}
}

static bool ShowModernM291ConfirmPopup(const Alert& alert)
{
	if (standardPopup == nullptr || mgr.IsPopupActive(standardPopup))
	{
		return false;
	}
	if (alertPopup != nullptr && mgr.IsPopupActive(alertPopup))
	{
		mgr.ClearPopup(true, alertPopup);
	}

	SetModernInfoPopupText(alert.title.c_str(), alert.text.c_str(), false);
	standardPopupContext = StandardPopupContext::M291Confirm;
	standardPopupM291Seq = alert.seq;
	standardPopupCancelButton->SetPosition(189, 332);
	standardPopupConfirmButton->SetPosition(362, 332);
	standardPopupCancelButton->Show(true);
	standardPopupConfirmButton->Show(true);
	mgr.SetPopup(standardPopup, AutoPlace, AutoPlace);
	return true;
}

static void RefreshModernM291AxisSelection()
{
	const Colour accent = GetModernAccentColour();
	for (size_t i = 0; i < standardPopupM291AxisButtonCount; ++i)
	{
		ModernTextButton * const button = standardPopupChoiceButtons[i];
		button->SetBorderVisible(standardPopupChoiceValues[i] == standardPopupM291SelectedAxis);
		button->SetBorderColour(accent);
	}
	mgr.Refresh(false);
}

static bool ShowModernM291ConfirmControlsPopup(const Alert& alert)
{
	if (standardPopup == nullptr || mgr.IsPopupActive(standardPopup))
	{
		return false;
	}
	if (alertPopup != nullptr && mgr.IsPopupActive(alertPopup))
	{
		mgr.ClearPopup(true, alertPopup);
	}

	// Collect the selectable axes first. If there are none, nothing has been
	// changed yet, so fall back to the plain confirmation popup. Otherwise the
	// M291 would block the printer with nothing on screen to acknowledge it.
	// (The list is kept locally because SetModernInfoPopupText() below clears
	// the shared choice-value array.)
	int controlAxes[10];
	size_t controlAxisCount = 0;
	for (size_t axis = 0; axis < MaxTotalAxes && controlAxisCount < ARRAY_SIZE(controlAxes); ++axis)
	{
		if ((alert.controls & (1u << axis)) != 0 && OM::GetAxis(axis) != nullptr)
		{
			controlAxes[controlAxisCount++] = (int)axis;
		}
	}
	if (controlAxisCount == 0)
	{
		return ShowModernM291ConfirmPopup(alert);
	}

	SetModernInfoPopupText(alert.title.c_str(), alert.text.c_str(), false);
	standardPopupContext = StandardPopupContext::M291ConfirmControls;
	standardPopupM291Seq = alert.seq;

	// Controlled confirmations reuse the shared button pool: up to ten axes
	// in one compact row and six fixed jog distances in the row below.
	standardPopupInfoCard->Show(false);
	const PixelNumber infoY[4] = {104, 126, 148, 170};
	for (size_t i = 0; i < ARRAY_SIZE(standardPopupInfoFields); ++i)
	{
		if (modernInfoText[i].strlen() != 0)
		{
			standardPopupInfoFields[i]->SetPosition(70, infoY[i]);
			standardPopupInfoFields[i]->Show(true);
		}
	}
	if (alert.title.strlen() != 0)
	{
		ConfigureStandardPopupTitle(alert.title.c_str(), true);
		standardPopupNameCard->Show(true);
		standardPopupNameField->Show(true);
	}

	const Colour tile = UTFT::fromRGB(28, 34, 43);
	const Colour text = UTFT::fromRGB(229, 232, 236);
	standardPopupM291AxisButtonCount = controlAxisCount;
	for (size_t i = 0; i < controlAxisCount; ++i)
	{
		standardPopupChoiceValues[i] = controlAxes[i];
	}

	const PixelNumber axisW = 52;
	const PixelNumber axisGap = 6;
	const PixelNumber axisTotalW = standardPopupM291AxisButtonCount * axisW + (standardPopupM291AxisButtonCount - 1) * axisGap;
	PixelNumber axisX = (660 - axisTotalW) / 2;
	for (size_t i = 0; i < standardPopupM291AxisButtonCount; ++i)
	{
		const int axis = standardPopupChoiceValues[i];
		OM::Axis * const omAxis = OM::GetAxis(axis);
		ModernTextButton * const button = standardPopupChoiceButtons[i];
		button->SetPosition(axisX + i * (axisW + axisGap), 190);
		button->SetPositionAndWidth(axisX + i * (axisW + axisGap), axisW);
		button->SetColours(text, tile);
		button->SetText(omAxis != nullptr ? omAxis->letter : "?");
		button->SetEvent(evStandardPopupChoice, 1000 + axis);
		button->Show(true);
	}

	standardPopupM291SelectedAxis = standardPopupChoiceValues[0];
	OM::Axis * const selectedAxis = OM::GetAxis(standardPopupM291SelectedAxis);
	standardPopupM291SelectedAxisLetter = (selectedAxis != nullptr && selectedAxis->letter[0] != '\0') ? selectedAxis->letter[0] : '\0';

	const PixelNumber jogW = 85;
	const PixelNumber jogGap = 10;
	const PixelNumber jogTotalW = 6 * jogW + 5 * jogGap;
	const PixelNumber jogX = (660 - jogTotalW) / 2;
	for (size_t i = 0; i < 6; ++i)
	{
		ModernTextButton * const button = standardPopupChoiceButtons[standardPopupM291AxisButtonCount + i];
		button->SetPosition(jogX + i * (jogW + jogGap), 255);
		button->SetPositionAndWidth(jogX + i * (jogW + jogGap), jogW);
		button->SetColours(text, tile);
		button->SetText(standardPopupM291JogText[i]);
		button->SetEvent(evStandardPopupChoice, 2000 + (int)i);
		button->SetBorderVisible(false);
		button->Show(true);
	}
	standardPopupChoiceCount = standardPopupM291AxisButtonCount + 6;
	RefreshModernM291AxisSelection();

	standardPopupCancelButton->SetPosition(189, 332);
	standardPopupConfirmButton->SetPosition(362, 332);
	standardPopupCancelButton->Show(true);
	standardPopupConfirmButton->Show(true);
	mgr.SetPopup(standardPopup, AutoPlace, AutoPlace);
	return true;
}

static bool ShowModernM291ChoicesPopup(const Alert& alert)
{
	if (standardPopup == nullptr || mgr.IsPopupActive(standardPopup))
	{
		return false;
	}
	if (alertPopup != nullptr && mgr.IsPopupActive(alertPopup))
	{
		mgr.ClearPopup(true, alertPopup);
	}

	SetModernInfoPopupText(alert.title.c_str(), alert.text.c_str(), false);
	standardPopupContext = StandardPopupContext::M291Choices;
	standardPopupM291Seq = alert.seq;

	// Choice dialogs need the vertical space normally occupied by the 550x150
	// information card. Reuse its four text fields without the card, then place
	// up to ten SHORT choice tiles in two rows of five.
	standardPopupInfoCard->Show(false);
	const PixelNumber infoY[4] = {106, 128, 150, 172};
	for (size_t i = 0; i < ARRAY_SIZE(standardPopupInfoFields); ++i)
	{
		if (modernInfoText[i].strlen() != 0)
		{
			standardPopupInfoFields[i]->SetPosition(70, infoY[i]);
			standardPopupInfoFields[i]->Show(true);
		}
	}
	if (alert.title.strlen() != 0)
	{
		ConfigureStandardPopupTitle(alert.title.c_str(), true);
		standardPopupNameCard->Show(true);
		standardPopupNameField->Show(true);
	}

	const Colour tile = UTFT::fromRGB(28, 34, 43);
	const Colour text = UTFT::fromRGB(229, 232, 236);
	standardPopupChoiceCount = (alert.choices_count < ARRAY_SIZE(standardPopupM291ChoiceText))
		? alert.choices_count : ARRAY_SIZE(standardPopupM291ChoiceText);
	for (size_t i = 0; i < standardPopupChoiceCount; ++i)
	{
		const PixelNumber x = 50 + (i % 5) * 115;
		const PixelNumber y = 198 + (i / 5) * 65;
		ModernTextButton * const button = standardPopupChoiceButtons[i];
		button->SetPosition(x, y);
		button->SetPositionAndWidth(x, 100);
		button->SetColours(text, tile);
		standardPopupM291ChoiceText[i].copy(alert.choices[i].c_str());
		button->SetText(standardPopupM291ChoiceText[i].c_str());
		button->SetEvent(evStandardPopupChoice, (int)i);
		standardPopupChoiceValues[i] = (int)i;
		button->Show(true);
	}

	standardPopupCancelButton->SetPosition(275, 332);
	standardPopupCancelButton->Show(true);
	standardPopupConfirmButton->Show(false);
	mgr.SetPopup(standardPopup, AutoPlace, AutoPlace);
	return true;
}

static void ConfigureModernM291ValuePrompt(const Alert& alert, StandardPopupContext context, const char *valueText)
{
	SetModernInfoPopupText(alert.title.c_str(), alert.text.c_str(), false);
	standardPopupContext = context;
	standardPopupM291Seq = alert.seq;
	standardPopupM291ValueText.copy(valueText);

	// Editable M291 prompts reuse the four information text rows without the
	// large card, leaving room for one LONG value tile above X/tick.
	standardPopupInfoCard->Show(false);
	const PixelNumber infoY[4] = {108, 133, 158, 183};
	for (size_t i = 0; i < ARRAY_SIZE(standardPopupInfoFields); ++i)
	{
		if (modernInfoText[i].strlen() != 0)
		{
			standardPopupInfoFields[i]->SetPosition(70, infoY[i]);
			standardPopupInfoFields[i]->Show(true);
		}
	}
	if (alert.title.strlen() != 0)
	{
		ConfigureStandardPopupTitle(alert.title.c_str(), true);
		standardPopupNameCard->Show(true);
		standardPopupNameField->Show(true);
	}

	const Colour tile = UTFT::fromRGB(28, 34, 43);
	const Colour text = UTFT::fromRGB(229, 232, 236);
	standardPopupChoiceCount = 1;
	ModernTextButton * const value = standardPopupChoiceButtons[0];
	value->SetPosition(230, 240);
	value->SetPositionAndWidth(230, 200);
	value->SetColours(text, tile);
	value->SetText(standardPopupM291ValueText.c_str());
	value->SetEvent(evStandardPopupChoice, 0);
	standardPopupChoiceValues[0] = 0;
	value->SetBorderVisible(true);
	value->SetBorderColour(GetModernAccentColour());
	value->Show(true);

	standardPopupCancelButton->SetPosition(189, 332);
	standardPopupConfirmButton->SetPosition(362, 332);
	standardPopupCancelButton->Show(true);
	standardPopupConfirmButton->Show(true);
}

static bool ShowModernM291NumberIntPopup(const Alert& alert)
{
	if (standardPopup == nullptr || mgr.IsPopupActive(standardPopup))
	{
		return false;
	}
	if (alertPopup != nullptr && mgr.IsPopupActive(alertPopup))
	{
		mgr.ClearPopup(true, alertPopup);
	}

	standardPopupM291IntMin = alert.limits.numberInt.min;
	standardPopupM291IntMax = alert.limits.numberInt.max;
	String<32> value;
	value.printf("%ld", (long)alert.limits.numberInt.valueDefault);
	ConfigureModernM291ValuePrompt(alert, StandardPopupContext::M291NumberInt, value.c_str());
	mgr.SetPopup(standardPopup, AutoPlace, AutoPlace);
	return true;
}

static void OpenM291IntegerNumpad()
{
	if (controlTempNumpadPopup == nullptr)
	{
		return;
	}
	numericPopupContext = NumericPopupContext::M291Int;
	numericPopupM291Text.copy(standardPopupM291ValueText.c_str());
	numericPopupM291Fresh = true;
	const Colour tile = UTFT::fromRGB(28, 34, 43);
	const Colour text = UTFT::fromRGB(229, 232, 236);
	const Colour dimText = UTFT::fromRGB(154, 164, 178);
	controlTempNumpadUnitField->Show(false);
	controlTempNumpadResourceField->SetPosition(395, 29);
	controlTempNumpadResourceField->SetPositionAndWidth(395, 160);
	controlTempNumpadResourceField->SetColours(text, tile);
	controlTempNumpadResourceField->SetText("INTEGER");
	controlTempNumpadDecimalButton->SetColours(dimText, tile);
	controlTempNumpadDecimalButton->SetEvent(evNull, 0);
	if (standardPopupM291IntMin < 0)
	{
		controlTempNumpadMinusButton->SetColours(text, tile);
		controlTempNumpadMinusButton->SetEvent(evNumericKey, 11);
	}
	else
	{
		controlTempNumpadMinusButton->SetColours(dimText, tile);
		controlTempNumpadMinusButton->SetEvent(evNull, 0);
	}
	controlTempNumpadValueField->SetValue(numericPopupM291Text.c_str());
	mgr.SetPopup(controlTempNumpadPopup, AutoPlace, AutoPlace);
}

static bool ShowModernM291NumberFloatPopup(const Alert& alert)
{
	if (standardPopup == nullptr || mgr.IsPopupActive(standardPopup))
	{
		return false;
	}
	if (alertPopup != nullptr && mgr.IsPopupActive(alertPopup))
	{
		mgr.ClearPopup(true, alertPopup);
	}

	standardPopupM291FloatMin = alert.limits.numberFloat.min;
	standardPopupM291FloatMax = alert.limits.numberFloat.max;
	String<32> value;
	value.printf("%f", (double)alert.limits.numberFloat.valueDefault);
	ConfigureModernM291ValuePrompt(alert, StandardPopupContext::M291NumberFloat, value.c_str());
	mgr.SetPopup(standardPopup, AutoPlace, AutoPlace);
	return true;
}

static void OpenM291FloatNumpad()
{
	if (controlTempNumpadPopup == nullptr)
	{
		return;
	}
	numericPopupContext = NumericPopupContext::M291Float;
	numericPopupM291Text.copy(standardPopupM291ValueText.c_str());
	numericPopupM291Fresh = true;
	const Colour tile = UTFT::fromRGB(28, 34, 43);
	const Colour text = UTFT::fromRGB(229, 232, 236);
	const Colour dimText = UTFT::fromRGB(154, 164, 178);
	controlTempNumpadUnitField->Show(false);
	controlTempNumpadResourceField->SetPosition(395, 29);
	controlTempNumpadResourceField->SetPositionAndWidth(395, 160);
	controlTempNumpadResourceField->SetColours(text, tile);
	controlTempNumpadResourceField->SetText("FLOAT");
	controlTempNumpadDecimalButton->SetColours(text, tile);
	controlTempNumpadDecimalButton->SetEvent(evNumericKey, 10);
	if (standardPopupM291FloatMin < 0.0f)
	{
		controlTempNumpadMinusButton->SetColours(text, tile);
		controlTempNumpadMinusButton->SetEvent(evNumericKey, 11);
	}
	else
	{
		controlTempNumpadMinusButton->SetColours(dimText, tile);
		controlTempNumpadMinusButton->SetEvent(evNull, 0);
	}
	controlTempNumpadValueField->SetValue(numericPopupM291Text.c_str());
	mgr.SetPopup(controlTempNumpadPopup, AutoPlace, AutoPlace);
}

static bool ShowModernM291TextPopup(const Alert& alert)
{
	if (standardPopup == nullptr || mgr.IsPopupActive(standardPopup))
	{
		return false;
	}
	if (alertPopup != nullptr && mgr.IsPopupActive(alertPopup))
	{
		mgr.ClearPopup(true, alertPopup);
	}

	standardPopupM291TextMin = alert.limits.text.min;
	standardPopupM291TextMax = alert.limits.text.max;
	ConfigureModernM291ValuePrompt(alert, StandardPopupContext::M291Text, alert.limits.text.valueDefault.c_str());
	mgr.SetPopup(standardPopup, AutoPlace, AutoPlace);
	return true;
}

static void OpenM291TextKeyboard()
{
	if (keyboardPopup == nullptr)
	{
		return;
	}
	keyboardDataHandler = M291TextData;
	currentHistoryBuffer = currentUserCommandBuffer;
	userCommandBuffers[currentUserCommandBuffer].copy(standardPopupM291ValueText.c_str());
	userCommandField->SetLabel(userCommandBuffers[currentUserCommandBuffer].c_str());
	userCommandField->SetChanged();
	mgr.SetPopup(keyboardPopup, AutoPlace, keyboardPopupY);
	keyboardIsDisplayed = true;
}

static void RefreshM291NumericValue()
{
	controlTempNumpadValueField->SetValue(numericPopupM291Text.c_str());
	const Colour tile = UTFT::fromRGB(28, 34, 43);
	const Colour text = UTFT::fromRGB(229, 232, 236);
	controlTempNumpadResourceField->SetColours(text, tile);
	controlTempNumpadResourceField->SetText(numericPopupContext == NumericPopupContext::M291Float ? "FLOAT" : "INTEGER");
}

static void ShowM291NumericRangeError()
{
	const Colour error = UTFT::fromRGB(201, 50, 24);
	const Colour glyph = UTFT::fromRGB(245, 245, 245);
	controlTempNumpadResourceField->SetColours(glyph, error);
	controlTempNumpadResourceField->SetText("OUT OF RANGE");
	mgr.GetPopup()->Refresh(false);
}

static void CreateControlMovementTabFields(const ColourScheme& colours)
{
	mgr.SetRoot(baseRoot);
	const Colour pageBg = UTFT::fromRGB(18, 22, 28);
	const Colour tile = UTFT::fromRGB(28, 34, 43);
	const Colour text = UTFT::fromRGB(229, 232, 236);
	const Colour muted = UTFT::fromRGB(154, 164, 178);
	const Colour neutralBorder = UTFT::fromRGB(59, 67, 79);
	DisplayField::SetDefaultFont(glcd19x21);

	// X/Y/Z position cards from the v7 SVG.
	const PixelNumber posSvgX[ControlMoveAxisCount] = { 118, 348, 578 };
	static const char * const axisLabels[ControlMoveAxisCount] = { "X:", "Y:", "Z:" };
	for (unsigned int axisSlot = 0; axisSlot < ControlMoveAxisCount; ++axisSlot)
	{
		const PixelNumber x = ControlX(posSvgX[axisSlot]);
		const PixelNumber w = ControlW(200);
		DisplayField::SetDefaultColours(muted, tile);
		DisplayField::SetDefaultFont(glcd19x21);
		mgr.AddField(new StaticTextField(98, x + ControlW(16), ControlW(44), TextAlignment::Left, axisLabels[axisSlot]));		// capitals of glcd19x21 are 9.5 px below the top: 98 + 9.5 = card centre
		controlMovePositionText[axisSlot].copy("---");
		DisplayField::SetDefaultColours(text, tile);
		DisplayField::SetDefaultFont(glcd28x32);
		controlMovePositionFields[axisSlot] = new StaticTextField(92, x + ControlW(55), w - ControlW(69), TextAlignment::Right, controlMovePositionText[axisSlot].c_str());		// digits of glcd28x32 are 15.5 px below the top: 92 + 15.5 = card centre
		mgr.AddField(controlMovePositionFields[axisSlot]);
		mgr.AddField(new ModernCard(82, x, w, 52, tile, neutralBorder, true));
	}

	DisplayField::SetDefaultColours(muted, pageBg);
	DisplayField::SetDefaultFont(glcd19x21);
	mgr.AddField(new StaticTextField(151, ControlX(118), ControlW(220), TextAlignment::Left, "MOVE STEPS:"));

	// Step tiles: 0.1 / 0.02 on the first row, then 1 / 10 / 50.
	const PixelNumber stepX[ControlMoveStepCount] = { 118, 234, 118, 118, 118 };
	const PixelNumber stepY[ControlMoveStepCount] = { 190, 190, 260, 330, 400 };
	for (unsigned int i = 0; i < ControlMoveStepCount; ++i)
	{
		DisplayField::SetDefaultColours(text, tile);
		controlMoveStepButtons[i] = new ModernTextButton(stepY[i], ControlX(stepX[i]), ControlW(110), 62,
			controlMoveStepText[i], evControlMoveStep, static_cast<int>(i), glcd28x32, false);
		mgr.AddField(controlMoveStepButtons[i]);
	}

	// Jog buttons.
	struct JogButtonDef { PixelNumber x, y; const char *label; int param; };
	static const JogButtonDef jogButtons[] = {
		{ 400, 190, "Y+", 3 }, { 664, 190, "Z+", 5 },
		{ 268, 285, "X-", 0 }, { 532, 285, "X+", 1 },
		{ 400, 380, "Y-", 2 }, { 664, 380, "Z-", 4 }
	};
	for (const JogButtonDef& def : jogButtons)
	{
		DisplayField::SetDefaultColours(text, tile);
		mgr.AddField(new ModernTextButton(def.y, ControlX(def.x), ControlW(117), 80,
			def.label, evControlMoveJog, def.param, glcd28x32));
	}

	// Bed compensation (G32 -> bed.g).  It intentionally has no Accent state.
	DisplayField::SetDefaultColours(text, tile);
	controlMoveBedCompButton = new ModernBedCompButton(190, ControlX(532), ControlW(117), 80, evControlMoveBedComp);
	mgr.AddField(controlMoveBedCompButton);

	// Home ALL, X, Y, Z.  RRF homed state controls both the Accent outline and
	// the vector home glyph colour.
	DisplayField::SetDefaultColours(UTFT::fromRGB(188, 196, 207), tile);
	// Home button inner labels ("ALL"/"X"/"Y"/"Z") stay at the smaller font
	// deliberately -- glcd28x32 is too wide to fit inside the house glyph's
	// wall opening, unlike every other tile on this page.
	controlMoveHomeAllButton = new ModernHomeButton(285, ControlX(400), ControlW(117), 80, "ALL", evControlMoveHome, 3, glcd19x21);
	controlMoveHomeButtons[0] = new ModernHomeButton(380, ControlX(268), ControlW(117), 80, "X", evControlMoveHome, 0, glcd19x21);
	controlMoveHomeButtons[1] = new ModernHomeButton(380, ControlX(532), ControlW(117), 80, "Y", evControlMoveHome, 1, glcd19x21);
	controlMoveHomeButtons[2] = new ModernHomeButton(285, ControlX(664), ControlW(117), 80, "Z", evControlMoveHome, 2, glcd19x21);
	mgr.AddField(controlMoveHomeAllButton);
	mgr.AddField(controlMoveHomeButtons[0]);
	mgr.AddField(controlMoveHomeButtons[1]);
	mgr.AddField(controlMoveHomeButtons[2]);

	controlMovementRoot = mgr.GetRoot();
	mgr.SetRoot(controlMovementRoot);
	AddTopTab(0, 4, "TOOLS", evControlTools);
	AddTopTab(1, 4, "MOVE", evControlMovement);
	AddTopTab(2, 4, "EXTRUDE", evControlExtrusion);
	AddTopTab(3, 4, "MACROS", evControlMacros);
	AddTopTabBackground();
	controlMovementRoot = mgr.GetRoot();

	// Page background is added last because Window::AddField prepends fields.
	mgr.SetRoot(controlMovementRoot);
	mgr.AddField(new ModernCard(0, masterTabWidth, DisplayX - masterTabWidth, DisplayY, pageBg, pageBg));
	controlMovementRoot = mgr.GetRoot();
	DisplayField::SetDefaultFont(DEFAULT_FONT);
	RefreshControlMoveSteps();
	RefreshControlMoveHoming();
	for (unsigned int axisSlot = 0; axisSlot < ControlMoveAxisCount; ++axisSlot)
	{
		RefreshControlMovePosition(axisSlot);
	}
	UNUSED(colours);
}

static unsigned int CountControlExtrudeTools()
{
	unsigned int count = 0;
	OM::IterateToolsWhile([&count](OM::Tool*&, size_t) {
		++count;
		return true;
	});
	return count;
}

static OM::Tool *GetControlExtrudeToolByOrdinal(unsigned int ordinal)
{
	OM::Tool *result = nullptr;
	unsigned int pos = 0;
	OM::IterateToolsWhile([&](OM::Tool*& tool, size_t) {
		if (pos++ == ordinal)
		{
			result = tool;
			return false;
		}
		return true;
	});
	return result;
}

static void SelectControlExtrudePageForActiveTool()
{
	if (currentTool < 0)
	{
		return;
	}
	unsigned int ordinal = 0;
	bool found = false;
	OM::IterateToolsWhile([&](OM::Tool*& tool, size_t) {
		if (tool != nullptr && tool->index == currentTool)
		{
			found = true;
			return false;
		}
		++ordinal;
		return true;
	});
	if (found)
	{
		controlExtrudeToolPage = ordinal / ControlExtrudeToolsPerPage;
	}
}

static void RefreshControlExtrudeTools()
{
	if (controlExtrudeActiveToolCard == nullptr)
	{
		return;
	}
	const Colour text = UTFT::fromRGB(229, 232, 236);
	const Colour tile = UTFT::fromRGB(28, 34, 43);
	const Colour accent = GetModernAccentColour();
	const OM::Tool * const tool = (currentTool >= 0) ? OM::GetTool(currentTool) : nullptr;
	if (tool == nullptr)
	{
		controlExtrudeActiveToolNameText.copy("OFF");
		controlExtrudeActiveToolTempText.copy("---" DEGREE_SYMBOL "C");
	}
	else
	{
		controlExtrudeActiveToolNameText.printf("T%d", tool->index);
		controlExtrudeActiveToolTempText.copy("---" DEGREE_SYMBOL "C");
		if (tool->heaters[0] != nullptr)
		{
			const unsigned int heater = tool->heaters[0]->heaterIndex;
			if (heater < JobStatusMaxHeaters && jobStatusHeaterValid[heater])
			{
				controlExtrudeActiveToolTempText.printf("%.1f" DEGREE_SYMBOL "C", (double)jobStatusHeaterTemps[heater]);
			}
		}
	}
	controlExtrudeActiveToolNameField->SetText(controlExtrudeActiveToolNameText.c_str());
	controlExtrudeActiveToolTempField->SetValue(controlExtrudeActiveToolTempText.c_str());
	// Accent border only when a tool is actually active, matching
	// controlToolHeaderCards' conditional border on CONTROL / TOOLS -- not
	// hard-coded on, so the OFF state reads as a normal, neutral tile.
	const bool active = (tool != nullptr);
	controlExtrudeActiveToolNameField->SetColours(active ? accent : text, tile);
	controlExtrudeActiveToolTempField->SetColours(active ? accent : text, tile);
	controlExtrudeActiveToolCard->SetBorderVisible(active);
	controlExtrudeActiveToolCard->SetBorderColour(accent);
}

static void RefreshControlExtrudeSelections()
{
	const Colour tile = UTFT::fromRGB(28, 34, 43);
	const Colour text = UTFT::fromRGB(229, 232, 236);
	const Colour accent = GetModernAccentColour();
	for (unsigned int i = 0; i < ControlExtrudeSpeedCount; ++i)
	{
		if (controlExtrudeSpeedButtons[i] != nullptr)
		{
			const bool selected = (i == controlExtrudeSelectedSpeed);
			controlExtrudeSpeedButtons[i]->SetColours(selected ? accent : text, tile);
			controlExtrudeSpeedButtons[i]->SetBorderColour(accent);
			controlExtrudeSpeedButtons[i]->SetBorderVisible(selected);
		}
	}
	for (unsigned int i = 0; i < ControlExtrudeDistanceCount; ++i)
	{
		if (controlExtrudeDistanceButtons[i] != nullptr)
		{
			const bool selected = (i == controlExtrudeSelectedDistance);
			controlExtrudeDistanceButtons[i]->SetColours(selected ? accent : text, tile);
			controlExtrudeDistanceButtons[i]->SetBorderColour(accent);
			controlExtrudeDistanceButtons[i]->SetBorderVisible(selected);
		}
	}
}

static bool ControlExtrudeTemperatureReady(bool retract)
{
	if (currentTool < 0)
	{
		ShowModernAlert("No active tool on shuttle !");
		return false;
	}
	OM::Tool * const tool = OM::GetTool(static_cast<size_t>(currentTool));
	if (tool == nullptr || tool->extruders.IsEmpty())
	{
		ShowModernAlert("No active tool on shuttle !");
		return false;
	}

	// coldRetractTemperature was added after coldExtrudeTemperature.  If an
	// older RRF omits it, use the extrusion threshold as the safe fallback.
	const bool thresholdValid = retract
		? (controlColdRetractTemperatureValid || controlColdExtrudeTemperatureValid)
		: controlColdExtrudeTemperatureValid;
	const float threshold = (retract && controlColdRetractTemperatureValid)
		? controlColdRetractTemperature
		: controlColdExtrudeTemperature;
	if (!thresholdValid)
	{
		ShowModernAlert("Nozzle temperature too low !");
		return false;
	}
	if (threshold <= 0.0f)
	{
		return true;
	}

	bool hasHeater = false;
	bool hotEnough = true;
	tool->IterateHeaters([&](OM::ToolHeater *heater, size_t) {
		if (heater == nullptr)
		{
			return;
		}
		hasHeater = true;
		const unsigned int heaterIndex = heater->heaterIndex;
		if (heaterIndex >= JobStatusMaxHeaters || !jobStatusHeaterValid[heaterIndex] ||
			jobStatusHeaterTemps[heaterIndex] < threshold)
		{
			hotEnough = false;
		}
	});
	if (!hasHeater || !hotEnough)
	{
		ShowModernAlert("Nozzle temperature too low !");
		return false;
	}
	return true;
}

static void SendControlExtrudeAction(bool retract)
{
	if (!ControlExtrudeTemperatureReady(retract))
	{
		return;
	}
	SerialIo::Sendf("M120 M83 G1 E%s%s F%d M121\n",
		retract ? "-" : "",
		controlExtrudeDistanceParam[controlExtrudeSelectedDistance],
		controlExtrudeSpeedFeedrate[controlExtrudeSelectedSpeed]);
}

static void CreateControlExtrusionTabFields(const ColourScheme& colours)
{
	mgr.SetRoot(baseRoot);
	const Colour pageBg = UTFT::fromRGB(18, 22, 28);
	const Colour tile = UTFT::fromRGB(28, 34, 43);
	const Colour actionTile = UTFT::fromRGB(42, 49, 60);
	const Colour text = UTFT::fromRGB(229, 232, 236);
	const Colour muted = UTFT::fromRGB(154, 164, 178);
	DisplayField::SetDefaultFont(glcd19x21);

	// Layout: 4 columns (Active tool 126, Speed 168, Distance 168,
	// Retract/Extrude 168) with 22px gaps, centred horizontally in the
	// 710px content pane (5px margin each side). Rows are 63 tall on a
	// 70px pitch, 4 rows starting at y=132, centred vertically between
	// the top tab bar (y=56) and the screen bottom (y=480).
	DisplayField::SetDefaultColours(muted, pageBg);
	mgr.AddField(new StaticTextField(112, ControlX(95), ControlW(126), TextAlignment::Left, "Active tool:"));
	mgr.AddField(new StaticTextField(112, ControlX(249), ControlW(168), TextAlignment::Left, "Speed: [mm/s]"));
	mgr.AddField(new StaticTextField(112, ControlX(439), ControlW(168), TextAlignment::Left, "Distance: [mm]"));

	// Single "active tool" tile, carbon-copied from CONTROL / TOOLS's
	// per-column header card -- same 126x114 size, same glcd28x32 font,
	// same conditional Accent border/text (see controlToolHeaderCards
	// and RefreshControlToolsPage in CreateControlToolsTabFields).
	// Informational only; tapping it does nothing. It is its own
	// column now, top-aligned with row 0, not stretched to span the
	// row grid the way the old 4-row list used to.
	{
		const PixelNumber x = ControlX(95);
		const PixelNumber w = ControlW(126);
		controlExtrudeActiveToolNameText.copy("OFF");
		controlExtrudeActiveToolTempText.copy("---" DEGREE_SYMBOL "C");
		DisplayField::SetDefaultColours(GetModernAccentColour(), tile);
		DisplayField::SetDefaultFont(glcd28x32);
		controlExtrudeActiveToolNameField = new ModernResourceLabel(144, x, w, 34, controlExtrudeActiveToolNameText.c_str(), glcd28x32);
		controlExtrudeActiveToolTempField = new StaticTextField(197, x, w, TextAlignment::Centre, controlExtrudeActiveToolTempText.c_str());
		mgr.AddField(controlExtrudeActiveToolNameField);
		mgr.AddField(controlExtrudeActiveToolTempField);
		controlExtrudeActiveToolCard = new ModernCard(132, x, w, 114, tile, GetModernAccentColour(), false);
		mgr.AddField(controlExtrudeActiveToolCard);
	}

	for (unsigned int i = 0; i < ControlExtrudeSpeedCount; ++i)
	{
		DisplayField::SetDefaultColours(text, tile);
		controlExtrudeSpeedButtons[i] = new ModernTextButton(132 + i * 70, ControlX(249), ControlW(168), 63,
			controlExtrudeSpeedText[i], evControlExtrudeSpeed, static_cast<int>(i), glcd28x32);
		mgr.AddField(controlExtrudeSpeedButtons[i]);
	}

	for (unsigned int i = 0; i < ControlExtrudeDistanceCount; ++i)
	{
		DisplayField::SetDefaultColours(text, tile);
		controlExtrudeDistanceButtons[i] = new ModernTextButton(132 + i * 70, ControlX(439), ControlW(168), 63,
			controlExtrudeDistanceText[i], evControlExtrudeDistance, static_cast<int>(i), glcd28x32);
		mgr.AddField(controlExtrudeDistanceButtons[i]);
	}

	// RETRACT at row 0's height, EXTRUDE at row 3's height, now the same
	// 168 width as Speed/Distance (was a cramped 98 before), rows 1-2 in
	// this column left empty, matching the rearranged mock-up.
	DisplayField::SetDefaultColours(text, actionTile);
	controlExtrudeRetractButton = new ModernTextButton(132, ControlX(629), ControlW(168), 63,
		"RETRACT", evControlExtrudeAction, -1, glcd19x21);
	controlExtrudeExtrudeButton = new ModernTextButton(132 + 3 * 70, ControlX(629), ControlW(168), 63,
		"EXTRUDE", evControlExtrudeAction, 1, glcd19x21);
	mgr.AddField(controlExtrudeRetractButton);
	mgr.AddField(controlExtrudeExtrudeButton);

	controlExtrusionRoot = mgr.GetRoot();
	mgr.SetRoot(controlExtrusionRoot);
	AddTopTab(0, 4, "TOOLS", evControlTools);
	AddTopTab(1, 4, "MOVE", evControlMovement);
	AddTopTab(2, 4, "EXTRUDE", evControlExtrusion);
	AddTopTab(3, 4, "MACROS", evControlMacros);
	AddTopTabBackground();
	controlExtrusionRoot = mgr.GetRoot();

	mgr.SetRoot(controlExtrusionRoot);
	mgr.AddField(new ModernCard(0, masterTabWidth, DisplayX - masterTabWidth, DisplayY, pageBg, pageBg));
	controlExtrusionRoot = mgr.GetRoot();
	DisplayField::SetDefaultFont(DEFAULT_FONT);
	SelectControlExtrudePageForActiveTool();
	RefreshControlExtrudeTools();
	RefreshControlExtrudeSelections();
	UNUSED(colours);
}

static PixelNumber TuneX(PixelNumber svgX)
{
	return contentLeft + static_cast<PixelNumber>((static_cast<uint32_t>(svgX - 90) * contentWidth) / 710);
}

static PixelNumber TuneW(PixelNumber svgW)
{
	const PixelNumber w = static_cast<PixelNumber>((static_cast<uint32_t>(svgW) * contentWidth) / 710);
	return (w == 0) ? 1 : w;
}

static PixelNumber JobX(PixelNumber svgX)
{
	return contentLeft + static_cast<PixelNumber>((static_cast<uint32_t>(svgX - 90) * contentWidth) / 710);
}

static PixelNumber JobW(PixelNumber svgW)
{
	const PixelNumber w = static_cast<PixelNumber>((static_cast<uint32_t>(svgW) * contentWidth) / 710);
	return (w == 0) ? 1 : w;
}

static PixelNumber ObjectX(PixelNumber svgX)
{
	return contentLeft + static_cast<PixelNumber>((static_cast<uint32_t>(svgX - 90) * contentWidth) / 710);
}

static PixelNumber ObjectW(PixelNumber svgW)
{
	const PixelNumber w = static_cast<PixelNumber>((static_cast<uint32_t>(svgW) * contentWidth) / 710);
	return (w == 0) ? 1 : w;
}

// True if a tool uses this fan as one of its own (part cooling) fans
static bool IsToolFan(unsigned int fan)
{
	bool found = false;
	OM::IterateToolsWhile([&found, fan](OM::Tool*& tool, size_t) {
		if (tool->fans.IsBitSet(fan))
		{
			found = true;
			return false;
		}
		return true;
	});
	return found;
}

// The general (non-tool) fans are recognised by their RRF fan names, not by fan number: FAN_AUX is the auxiliary fan and
// FAN_CHA the chamber/filter fan. Fans that belong to a tool are never returned. Used by the PRINTING tiles and by TUNE.
static int FindNamedFan(const char *name)
{
	for (unsigned int i = 0; i < TuneMaxFans; ++i)
	{
		if (tuneFanValid[i] && strcasecmp(tuneFanNames[i].c_str(), name) == 0 && !IsToolFan(i))
		{
			return static_cast<int>(i);
		}
	}
	return -1;
}

static int GetJobStatusToolExtruder()
{
	if (currentTool < 0)
	{
		return -1;
	}
	OM::Tool * const tool = OM::GetTool(static_cast<size_t>(currentTool));
	return (tool != nullptr && !tool->extruders.IsEmpty()) ? static_cast<int>(tool->extruders.LowestSetBit()) : -1;
}

// The fan that cools the print for a tool: the lowest-numbered fan of the tool that is NOT thermostatically controlled.
// A tool's fan list can also contain a heat-break fan that is thermostatic and shared with other tools (often fan 0). It
// must not be treated as the part cooling fan: setting its speed does nothing useful and it is the same fan for every tool
// that lists it. Falls back to the lowest fan if all of the tool's fans are thermostatic or that is not known yet.
static int GetToolPartFan(const OM::Tool *tool)
{
	if (tool == nullptr || tool->fans.IsEmpty())
	{
		return -1;
	}
	int lowest = -1;
	for (unsigned int fan = 0; fan < TuneMaxFans; ++fan)
	{
		if (tool->fans.IsBitSet(fan))
		{
			if (lowest < 0)
			{
				lowest = static_cast<int>(fan);
			}
			if (!tuneFanThermostatic[fan])
			{
				return static_cast<int>(fan);
			}
		}
	}
	return lowest;
}

static int GetJobStatusToolFan()
{
	if (currentTool < 0)
	{
		return -1;
	}
	OM::Tool * const tool = OM::GetTool(static_cast<size_t>(currentTool));
	return GetToolPartFan(tool);
}

static int GetJobStatusToolHeater()
{
	if (currentTool < 0)
	{
		return -1;
	}
	OM::Tool * const tool = OM::GetTool(static_cast<size_t>(currentTool));
	if (tool == nullptr || tool->heaters[0] == nullptr)
	{
		return -1;
	}
	return static_cast<int>(tool->heaters[0]->heaterIndex);
}

static int GetJobStatusBedHeater()
{
	OM::Bed * const bed = OM::GetFirstBed();
	return (bed != nullptr) ? static_cast<int>(bed->heater) : -1;
}

static int GetJobStatusChamberHeater()
{
	OM::Chamber * const chamber = OM::GetFirstChamber();
	return (chamber != nullptr) ? static_cast<int>(chamber->heater) : -1;
}

static const char *JobStatusTileLabel(JobStatusTileType type, String<20>& label)
{
	switch (type)
	{
	case JobStatusTileType::ToolTemp:
		if (currentTool >= 0)
		{
			label.printf("TOOL T%d", currentTool);
		}
		else
		{
			label.copy("TOOL T#");
		}
		break;
	case JobStatusTileType::BedTemp: label.copy("BED"); break;
	case JobStatusTileType::ChamberTemp: label.copy("CHAMBER"); break;
	case JobStatusTileType::FanPart:
		label.copy("FAN PART");		// always the part cooling fan of the selected tool, so the tile label does not change with the tool
		break;
	case JobStatusTileType::FanAux: label.copy("FAN AUX"); break;
	case JobStatusTileType::FanCha: label.copy("FAN ->I->"); break;
	case JobStatusTileType::SpeedReq: label.copy("SPEED REQ:"); break;
	case JobStatusTileType::SpeedCur: label.copy("SPEED"); break;
	case JobStatusTileType::FlowFactor: label.copy("FLOW"); break;
	case JobStatusTileType::FlowVol: label.copy("VOL. FLOW"); break;
	}
	return label.c_str();
}

static void RefreshJobStatusTile(unsigned int slot)
{
	if (slot >= JobStatusTileCount || jobStatusLabels[slot] == nullptr || jobStatusValues[slot] == nullptr)
	{
		return;
	}
	const JobStatusTileType type = jobStatusTiles[slot];
	jobStatusLabels[slot]->SetValue(JobStatusTileLabel(type, jobStatusLabelText[slot]));
	String<24>& value = jobStatusValueText[slot];
	value.copy("---");

	int index = -1;
	int heaterIndex = -1;
	switch (type)
	{
	case JobStatusTileType::ToolTemp:
		index = GetJobStatusToolHeater();
		heaterIndex = index;
		if (index >= 0 && index < static_cast<int>(JobStatusMaxHeaters) && jobStatusHeaterValid[index])
		{
			value.printf("%.1f" DEGREE_SYMBOL "C", (double)jobStatusHeaterTemps[index]);
		}
		break;
	case JobStatusTileType::BedTemp:
		index = GetJobStatusBedHeater();
		heaterIndex = index;
		if (index >= 0 && index < static_cast<int>(JobStatusMaxHeaters) && jobStatusHeaterValid[index])
		{
			value.printf("%.1f" DEGREE_SYMBOL "C", (double)jobStatusHeaterTemps[index]);
		}
		break;
	case JobStatusTileType::ChamberTemp:
		index = GetJobStatusChamberHeater();
		heaterIndex = index;
		if (index >= 0 && index < static_cast<int>(JobStatusMaxHeaters) && jobStatusHeaterValid[index])
		{
			value.printf("%.1f" DEGREE_SYMBOL "C", (double)jobStatusHeaterTemps[index]);
		}
		break;
	case JobStatusTileType::FanPart:
		index = GetJobStatusToolFan();
		if (index >= 0 && index < static_cast<int>(TuneMaxFans) && tuneFanValid[index])
		{
			value.printf("%d%%", tuneFanPercent[index]);
		}
		break;
	case JobStatusTileType::FanAux:
		index = FindNamedFan("FAN_AUX");
		if (index >= 0) value.printf("%d%%", tuneFanPercent[index]);
		break;
	case JobStatusTileType::FanCha:
		index = FindNamedFan("FAN_CHA");
		if (index >= 0) value.printf("%d%%", tuneFanPercent[index]);
		break;
	case JobStatusTileType::SpeedReq:
		value.printf("%.0f mm/s", (double)jobStatusRequestedSpeed);
		break;
	case JobStatusTileType::SpeedCur:
		value.printf("%.0f mm/s", (double)jobStatusTopSpeed);
		break;
	case JobStatusTileType::FlowFactor:
		index = GetJobStatusToolExtruder();
		if (index >= 0 && index < static_cast<int>(TuneMaxExtruders))
		{
			value.printf("%d%%", tuneExtruderFactor[index]);
		}
		break;
	case JobStatusTileType::FlowVol:
		index = GetJobStatusToolExtruder();
		if (index >= 0 && index < static_cast<int>(JobStatusMaxExtruders) && jobStatusFilamentDiameterValid[index])
		{
			const float d = jobStatusFilamentDiameter[index];
			const float area = 0.7853981634f * d * d;
			const bool haveRate = jobStatusExtrusionRateHaveAvg
				&& (SystemTick::GetTickCount() - jobStatusExtrusionRateTime) <= JobStatusFlowStaleMs;
			value.printf("%.1f mm3/s", (double)((haveRate ? jobStatusExtrusionRateAvg : 0.0f) * area));
		}
		break;
	}

	const Colour normalTile = UTFT::fromRGB(28, 34, 43);
	const Colour muted = UTFT::fromRGB(154, 164, 178);
	const Colour text = UTFT::fromRGB(229, 232, 236);
	const Colour heaterFault = UTFT::fromRGB(159, 62, 255);
	const bool fault = heaterIndex >= 0 && heaterIndex < static_cast<int>(JobStatusMaxHeaters) &&
		jobStatusHeaterStatus[heaterIndex] == OM::HeaterStatus::fault;
	const Colour cardColour = fault ? heaterFault : normalTile;
	jobStatusCards[slot]->SetFillColour(cardColour);
	jobStatusLabels[slot]->SetColours(muted, cardColour);
	jobStatusValues[slot]->SetColours(text, cardColour);
	jobStatusValues[slot]->SetValue(value.c_str());
}

static void RefreshJobStatusTiles()
{
	for (unsigned int i = 0; i < JobStatusTileCount; ++i)
	{
		RefreshJobStatusTile(i);
	}
}

static void RefreshJobStatusTilesByType(JobStatusTileType type)
{
	for (unsigned int i = 0; i < JobStatusTileCount; ++i)
	{
		if (jobStatusTiles[i] == type)
		{
			RefreshJobStatusTile(i);
		}
	}
}

static void RefreshJobStatusHeader()
{
	if (jobStatusNameField != nullptr)
	{
		jobStatusNameText.copy(printingFile.IsEmpty() ? "No active print" : printingFile.c_str());
		jobStatusNameField->SetValue(jobStatusNameText.c_str());
	}
	if (jobStatusProgressField != nullptr)
	{
		jobStatusProgressText.printf("%u%%", constrain<unsigned int>(jobStatusProgress, 0, 100));
		jobStatusProgressField->SetValue(jobStatusProgressText.c_str());
	}
	if (jobStatusLayersField != nullptr)
	{
		if (jobStatusNumLayers != 0)
		{
			jobStatusLayersText.printf("%u / %u", jobStatusLayer, jobStatusNumLayers);
		}
		else
		{
			jobStatusLayersText.printf("%u / ---", jobStatusLayer);
		}
		jobStatusLayersField->SetValue(jobStatusLayersText.c_str());
	}
	if (jobStatusTimeField != nullptr)
	{
		const uint32_t hours = jobStatusDuration / 3600;
		const uint32_t mins = (jobStatusDuration / 60) % 60;
		const uint32_t secs = jobStatusDuration % 60;
		jobStatusTimeText.printf("%lu : %02lu : %02lu", (unsigned long)hours, (unsigned long)mins, (unsigned long)secs);
		jobStatusTimeField->SetValue(jobStatusTimeText.c_str());
	}
}

static void RefreshJobStatusActions()
{
	if (jobStatusPauseResumeButton == nullptr || jobStatusAbortButton == nullptr)
	{
		return;
	}

	const OM::PrinterStatus stat = GetStatus();
	const bool paused = (stat == OM::PrinterStatus::paused || stat == OM::PrinterStatus::resuming);
	const bool canPauseResume = (stat == OM::PrinterStatus::printing || stat == OM::PrinterStatus::paused ||
		stat == OM::PrinterStatus::pausing || stat == OM::PrinterStatus::resuming);
	const bool canAbort = canPauseResume || stat == OM::PrinterStatus::simulating;

	jobStatusPauseResumeButton->SetText(paused ? "> RESUME" : "|| PAUSE");

	// Hidden PRINTING actions must also be non-interactive.  Clearing the event
	// prevents an invisible button from ever being selected by the touch hit-test,
	// and clearing Press() prevents a stale outline/fill when it is shown again.
	if (canPauseResume)
	{
		jobStatusPauseResumeButton->SetEvent(evStatusJobStatusPauseResume, 0);
	}
	else
	{
		jobStatusPauseResumeButton->Press(false, 0);
		jobStatusPauseResumeButton->SetEvent(nullEvent, 0);
	}
	mgr.Show(jobStatusPauseResumeButton, canPauseResume);

	if (canAbort)
	{
		jobStatusAbortButton->SetEvent(evStatusJobStatusAbort, 0);
	}
	else
	{
		jobStatusAbortButton->Press(false, 0);
		jobStatusAbortButton->SetEvent(nullEvent, 0);
	}
	mgr.Show(jobStatusAbortButton, canAbort);
}

static void JobStatusThumbnailRefreshNotify(bool full, bool changed)
{
	if ((!full && !changed) || printingFile.IsEmpty() || !PrintInProgress())
	{
		return;
	}
	SerialIo::Sendf(GetFirmwareFeatures().IsBitSet(noM20M36) ? "M408 S36 P\"%s\"\n" : "M36 \"%s\"\n", printingFile.c_str());
}

// RRF only cancels a job that is paused (sending M0 to a running print just produces an error saying that the print
// must be paused first). So when ABORT is confirmed while the job is running, pause it first (M25) and send the
// cancel (M0) as soon as the printer reports that it is paused. Give up if that takes too long.
static bool jobAbortWhenPaused = false;
static uint32_t jobAbortRequestedAt = 0;
static constexpr uint32_t JobAbortPauseTimeoutMs = 60000;

static void RequestJobAbort()
{
	switch (GetStatus())
	{
	case OM::PrinterStatus::printing:
	case OM::PrinterStatus::simulating:
	case OM::PrinterStatus::resuming:
		SerialIo::Sendf("M25\n");
		jobAbortWhenPaused = true;
		jobAbortRequestedAt = SystemTick::GetTickCount();
		break;

	case OM::PrinterStatus::pausing:
		// A pause is already in progress: just wait for it to finish
		jobAbortWhenPaused = true;
		jobAbortRequestedAt = SystemTick::GetTickCount();
		break;

	default:
		SerialIo::Sendf("M0\n");					// already paused (or not in a state where pausing makes sense)
		break;
	}
}

static void OpenJobStatusConfirmation(JobStatusConfirmAction action)
{
	jobStatusConfirmAction = action;
	switch (action)
	{
	case JobStatusConfirmAction::Pause: jobStatusConfirmText.copy("Do you want to PAUSE this print?"); break;
	case JobStatusConfirmAction::Resume: jobStatusConfirmText.copy("Do you want to RESUME this print?"); break;
	case JobStatusConfirmAction::Abort: jobStatusConfirmText.copy("Do you want to ABORT this print?"); break;
	default: jobStatusConfirmText.copy("Are you sure?"); break;
	}
	standardPopupContext = StandardPopupContext::JobStatusConfirm;
	ConfigureStandardPopupTitle("ALERT !", false);
	ResetStandardPopupContent();
	ConfigureStandardPopupInformation(jobStatusConfirmText.c_str());
	standardPopupCancelButton->Show(true);
	standardPopupConfirmButton->Show(true);
	mgr.SetPopup(standardPopup, AutoPlace, AutoPlace);
}

static void CreateStatusJobStatusTabFields(const ColourScheme& colours)
{
	mgr.SetRoot(baseRoot);
	const Colour pageBg = UTFT::fromRGB(18, 22, 28);
	const Colour tile = UTFT::fromRGB(28, 34, 43);
	const Colour text = UTFT::fromRGB(229, 232, 236);
	const Colour muted = UTFT::fromRGB(154, 164, 178);
	const Colour pauseCyan = UTFT::fromRGB(95, 195, 220);
	const Colour pauseText = UTFT::fromRGB(36, 36, 36);        // #242424
	const Colour abortRed = UTFT::fromRGB(201, 50, 24);
	const Colour abortText = UTFT::fromRGB(36, 36, 36);        // #242424

	DisplayField::SetDefaultFont(glcd19x21);

	// Job name and compact file-progress tile. Text is registered before the card because
	// Window::AddField prepends fields, so the card is painted first and the text on top.
	DisplayField::SetDefaultColours(text, tile);
	jobStatusNameField = new StaticTextField(91, JobX(132), JobW(544), TextAlignment::Left, "No active print");
	mgr.AddField(jobStatusNameField);
	jobStatusNameCard = new ModernCard(80, JobX(118), JobW(572), 44, tile, tile);
	mgr.AddField(jobStatusNameCard);
	jobStatusProgressField = new StaticTextField(91, JobX(700), JobW(80), TextAlignment::Centre, "0%");
	mgr.AddField(jobStatusProgressField);
	jobStatusProgressCard = new ModernCard(80, JobX(700), JobW(80), 44, tile, tile);
	mgr.AddField(jobStatusProgressCard);

	// Thumbnail card and direct-draw target. QOI pixels are converted to RGB565 as they arrive.
	jobStatusThumbnail = new DrawDirect(151, JobX(118), 220, 220, JobStatusThumbnailRefreshNotify);
	mgr.AddField(jobStatusThumbnail);
	jobStatusThumbnailCard = new ModernCard(151, JobX(118), 220, 220, tile, tile);
	mgr.AddField(jobStatusThumbnailCard);

	// 3x3 read-only information grid.
	for (unsigned int i = 0; i < JobStatusTileCount; ++i)
	{
		const unsigned int row = i / 3;
		const unsigned int col = i % 3;
		const PixelNumber x = JobX(358 + col * 146);
		const PixelNumber y = 151 + row * 77;
		const PixelNumber w = JobW(130);

		DisplayField::SetDefaultColours(muted, tile);
		jobStatusLabels[i] = new StaticTextField(y + 10, x, w, TextAlignment::Centre, "");
		mgr.AddField(jobStatusLabels[i]);
		DisplayField::SetDefaultColours(text, tile);
		jobStatusValues[i] = new StaticTextField(y + 37, x, w, TextAlignment::Centre, "---");
		mgr.AddField(jobStatusValues[i]);
		jobStatusCards[i] = new ModernCard(y, x, w, 67, tile, tile);
		mgr.AddField(jobStatusCards[i]);
	}

	// Bottom information fields.
	DisplayField::SetDefaultColours(text, tile);
	jobStatusLayersField = new StaticTextField(421, JobX(118), JobW(150), TextAlignment::Centre, "0 / ---");
	mgr.AddField(jobStatusLayersField);
	mgr.AddField(new ModernCard(407, JobX(118), JobW(150), 50, tile, tile));
	jobStatusTimeField = new StaticTextField(421, JobX(288), JobW(150), TextAlignment::Centre, "0 : 00 : 00");
	mgr.AddField(jobStatusTimeField);
	mgr.AddField(new ModernCard(407, JobX(288), JobW(150), 50, tile, tile));
	DisplayField::SetDefaultColours(muted, pageBg);
	mgr.AddField(new StaticTextField(387, JobX(118), JobW(150), TextAlignment::Left, "Layers:"));
	mgr.AddField(new StaticTextField(387, JobX(288), JobW(150), TextAlignment::Left, "Time:"));

	DisplayField::SetDefaultColours(pauseText, pauseCyan);
	jobStatusPauseResumeButton = new ModernTextButton(407, JobX(490), JobW(140), 50, "|| PAUSE", evStatusJobStatusPauseResume, 0, glcd19x21);
	mgr.AddField(jobStatusPauseResumeButton);
	DisplayField::SetDefaultColours(abortText, abortRed);
	jobStatusAbortButton = new ModernTextButton(407, JobX(640), JobW(140), 50, "ABORT!", evStatusJobStatusAbort, 0, glcd19x21);
	mgr.AddField(jobStatusAbortButton);

	statusJobStatusRoot = mgr.GetRoot();
	AddStatusSubTabs(statusJobStatusRoot);
	mgr.SetRoot(statusJobStatusRoot);
	mgr.AddField(new ModernCard(0, masterTabWidth, DisplayX - masterTabWidth, DisplayY, pageBg, pageBg));
	statusJobStatusRoot = mgr.GetRoot();
	DisplayField::SetDefaultFont(DEFAULT_FONT);
	RefreshJobStatusTiles();
	RefreshJobStatusHeader();
	RefreshJobStatusActions();
}

static void CreateControlMacrosTabFields(const ColourScheme& colours)
{
	mgr.SetRoot(baseRoot);
	const Colour pageBg = UTFT::fromRGB(18, 22, 28);
	const Colour tile = UTFT::fromRGB(28, 34, 43);
	const Colour text = UTFT::fromRGB(229, 232, 236);

	DisplayField::SetDefaultFont(glcd19x21);
	DisplayField::SetDefaultColours(text, tile);
	for (unsigned int row = 0; row < ControlMacroRows; ++row)
	{
		const PixelNumber y = 85 + row * 62;
		controlMacroFileButtons[row] = new ModernTextButton(y, ControlX(118), ControlW(598), 56,
			nullptr, evNull, 0, glcd19x21, false, TextAlignment::Left);
		mgr.AddField(controlMacroFileButtons[row]);
		mgr.Show(controlMacroFileButtons[row], false);
	}

	controlMacroPageUpButton = new ModernIconButton(147, ControlX(736), ControlW(54), 118, IconUp, evControlMacroPageUp);
	controlMacroPageDownButton = new ModernIconButton(271, ControlX(736), ControlW(54), 118, IconDown, evControlMacroPageDown);
	mgr.AddField(controlMacroPageUpButton);
	mgr.AddField(controlMacroPageDownButton);
	mgr.Show(controlMacroPageUpButton, false);
	mgr.Show(controlMacroPageDownButton, false);

	controlMacrosRoot = mgr.GetRoot();
	mgr.SetRoot(controlMacrosRoot);
	AddTopTab(0, 4, "TOOLS", evControlTools);
	AddTopTab(1, 4, "MOVE", evControlMovement);
	AddTopTab(2, 4, "EXTRUDE", evControlExtrusion);
	AddTopTab(3, 4, "MACROS", evControlMacros);
	AddTopTabBackground();
	controlMacrosRoot = mgr.GetRoot();

	// Page background is added last because Window::AddField prepends fields.
	mgr.SetRoot(controlMacrosRoot);
	mgr.AddField(new ModernCard(0, masterTabWidth, DisplayX - masterTabWidth, DisplayY, pageBg, pageBg));
	controlMacrosRoot = mgr.GetRoot();
	DisplayField::SetDefaultFont(DEFAULT_FONT);
}

static void ConfigureStatusJobStartPopupContent()
{
	standardPopupContext = StandardPopupContext::StatusJobStart;
	ConfigureStandardPopupTitle("START PRINT", false);
	ResetStandardPopupContent();
	ConfigureStandardPopupInformation("Do you want to start this print?", currentFile != nullptr ? currentFile : "");

	// Three-action layout: Trash / X / tick.
	standardPopupTrashButton->SetPosition(102, 332);
	standardPopupTrashButton->Show(true);
	standardPopupCancelButton->SetPosition(275, 332);
	standardPopupConfirmButton->SetPosition(448, 332);
	standardPopupCancelButton->Show(true);
	standardPopupConfirmButton->Show(true);
}

static void ConfigureStatusJobDeletePopupContent()
{
	standardPopupContext = StandardPopupContext::StatusJobDelete;
	ConfigureStandardPopupTitle("DELETE JOB", false);
	ResetStandardPopupContent();
	ConfigureStandardPopupInformation("Do you want to delete this job?", currentFile != nullptr ? currentFile : "");
	standardPopupCancelButton->Show(true);
	standardPopupConfirmButton->Show(true);
}

static void CreateStatusJobTabFields(const ColourScheme& colours)
{
	UNUSED(colours);
	mgr.SetRoot(baseRoot);
	const Colour pageBg = UTFT::fromRGB(18, 22, 28);
	const Colour tile = UTFT::fromRGB(28, 34, 43);
	const Colour text = UTFT::fromRGB(229, 232, 236);
	const Colour accent = GetModernAccentColour();

	DisplayField::SetDefaultFont(glcd19x21);
	DisplayField::SetDefaultColours(text, tile);
	for (unsigned int row = 0; row < StatusJobRows; ++row)
	{
		const PixelNumber y = 85 + row * 62;
		statusJobFileButtons[row] = new ModernTextButton(y, JobX(118), JobW(598), 56, nullptr, evNull, 0, glcd19x21, false, TextAlignment::Left);
		mgr.AddField(statusJobFileButtons[row]);
		mgr.Show(statusJobFileButtons[row], false);
	}

	// Optional storage-volume selector. It occupies the first-row slot in the
	// same right-hand column as the page arrows and is shown only when RRF
	// reports more than one *mounted* volume. Empty/unmounted card slots stay hidden.
	DisplayField::SetDefaultColours(accent, tile);
	statusJobSdButton = new ModernTextButton(85, JobX(736), JobW(54), 56, "SD", evChangeCard, 0, glcd19x21, true);
	statusJobSdButton->SetBorderColour(accent);
	mgr.AddField(statusJobSdButton);
	mgr.Show(statusJobSdButton, statusJobNumVolumes > 1);

	DisplayField::SetDefaultColours(text, tile);
	statusJobPageUpButton = new ModernIconButton(147, JobX(736), JobW(54), 118, IconUp, evStatusJobPageUp);
	statusJobPageDownButton = new ModernIconButton(271, JobX(736), JobW(54), 118, IconDown, evStatusJobPageDown);
	mgr.AddField(statusJobPageUpButton);
	mgr.AddField(statusJobPageDownButton);
	mgr.Show(statusJobPageUpButton, false);
	mgr.Show(statusJobPageDownButton, false);

	statusJobRoot = mgr.GetRoot();
	AddStatusSubTabs(statusJobRoot);

	// Paint the content area behind the JOB fields, matching the modern STATUS pages.
	mgr.SetRoot(statusJobRoot);
	DisplayField::SetDefaultColours(text, pageBg);
	mgr.AddField(new ModernTextButton(0, masterTabWidth, DisplayX - masterTabWidth, DisplayY, nullptr, evNull, 0, glcd19x21));
	statusJobRoot = mgr.GetRoot();
	DisplayField::SetDefaultFont(DEFAULT_FONT);

}

static void UpdateTunePopupValue()
{
	if (tunePopupKind == TunePopupKind::PressureAdvance)
	{
		tunePopupValueText.printf("%.4f", (double)tunePopupPa);
		if (standardPopupContext == StandardPopupContext::TunePressureAdvance)
		{
			standardPopupChoiceButtons[0]->SetText(tunePopupValueText.c_str());
		}
	}
	else
	{
		tunePopupValueText.printf("%d %%", tunePopupPercent);
		ModernTextButton *value = nullptr;
		if (tunePopupKind == TunePopupKind::Fan)
		{
			value = (standardPopupContext == StandardPopupContext::TuneFan) ? standardPopupChoiceButtons[0] : nullptr;
		}
		else if (tunePopupKind == TunePopupKind::Speed)
		{
			value = (standardPopupContext == StandardPopupContext::TuneSpeed) ? standardPopupChoiceButtons[0] : nullptr;
		}
		else
		{
			value = (standardPopupContext == StandardPopupContext::TuneFlow) ? standardPopupChoiceButtons[0] : nullptr;
		}
		if (value != nullptr)
		{
			value->SetText(tunePopupValueText.c_str());
		}
	}
}

static void ConfigureStandardTunePopup(StandardPopupContext context, const char *title, bool longTitle,
	const char * const labels[4], const int deltas[4], Event adjustEvent)
{
	const Colour tile = UTFT::fromRGB(28, 34, 43);
	const Colour text = UTFT::fromRGB(229, 232, 236);
	const Colour neutralBorder = UTFT::fromRGB(59, 67, 79);
	static const PixelNumber adjustX[4] = { 70, 210, 350, 490 };

	standardPopupContext = context;
	ConfigureStandardPopupTitle(title, longTitle);
	ResetStandardPopupContent();

	// Slot 0 is reused as the read-only MEDIUM value tile.
	ModernTextButton * const value = standardPopupChoiceButtons[0];
	value->SetPosition(255, 139);
	value->SetPositionAndWidth(255, 150);
	value->SetColours(text, tile);
	value->SetText("");
	value->SetEvent(evNull, 0);
	value->SetBorderColour(neutralBorder);
	value->SetBorderVisible(true);
	value->Show(true);

	// Slots 1..4 are the four SHORT adjustment buttons.
	for (size_t i = 0; i < 4; ++i)
	{
		ModernTextButton * const button = standardPopupChoiceButtons[i + 1];
		button->SetPosition(adjustX[i], 219);
		button->SetPositionAndWidth(adjustX[i], 100);
		button->SetColours(text, tile);
		button->SetText(labels[i]);
		button->SetEvent(adjustEvent, deltas[i]);
		button->SetBorderColour(neutralBorder);
		button->SetBorderVisible(true);
		button->Show(true);
	}

	standardPopupCancelButton->Show(true);
	standardPopupConfirmButton->Show(true);
}

static void OpenTunePressureAdvancePopup(int toolIndex, int extruder)
{
	static const int deltas[4] = { -10, -2, 2, 10 }; // thousandths
	static const char * const labels[4] = { "-0.01", "-0.002", "+0.002", "+0.01" };
	if (extruder < 0 || extruder >= (int)TuneMaxExtruders)
	{
		return;
	}
	tunePopupKind = TunePopupKind::PressureAdvance;
	tunePopupResource = extruder;
	tunePopupPa = tunePressureAdvance[extruder];
	tunePopupTitleText.printf("PRESSURE ADV. T%d", toolIndex);
	ConfigureStandardTunePopup(StandardPopupContext::TunePressureAdvance, tunePopupTitleText.c_str(), true, labels, deltas, evTunePopupAdjustPa);
	UpdateTunePopupValue();
	mgr.SetPopup(standardPopup, AutoPlace, AutoPlace);
}

static void OpenTuneFanPopup(const char *title, int toolIndex, int fanIndex)
{
	static const int deltas[4] = { -10, -5, 5, 10 };
	static const char * const labels[4] = { "-10", "-5", "+5", "+10" };
	UNUSED(toolIndex);
	if (fanIndex < 0 || fanIndex >= (int)TuneMaxFans)
	{
		return;
	}
	tunePopupKind = TunePopupKind::Fan;
	tunePopupResource = fanIndex;
	tunePopupPercent = tuneFanPercent[fanIndex];
	tunePopupTitleText.copy(title);
	ConfigureStandardTunePopup(StandardPopupContext::TuneFan, tunePopupTitleText.c_str(), true, labels, deltas, evTunePopupAdjustPercent);
	UpdateTunePopupValue();
	mgr.SetPopup(standardPopup, AutoPlace, AutoPlace);
}

static void OpenTuneSpeedPopup()
{
	static const int deltas[4] = { -10, -5, 5, 10 };
	static const char * const labels[4] = { "-10", "-5", "+5", "+10" };
	tunePopupKind = TunePopupKind::Speed;
	tunePopupResource = -1;
	tunePopupPercent = tuneSpeedPercent;
	ConfigureStandardTunePopup(StandardPopupContext::TuneSpeed, "SPEED", false, labels, deltas, evTunePopupAdjustPercent);
	UpdateTunePopupValue();
	mgr.SetPopup(standardPopup, AutoPlace, AutoPlace);
}

static void OpenTuneFeedPopup(int toolIndex, int extruder)
{
	static const int deltas[4] = { -3, -1, 1, 3 };
	static const char * const labels[4] = { "-3", "-1", "+1", "+3" };
	tunePopupKind = TunePopupKind::Flow;
	tunePopupResource = extruder;
	tunePopupPercent = (extruder >= 0 && extruder < (int)TuneMaxExtruders) ? tuneExtruderFactor[extruder] : 100;
	tunePopupTitleText.printf("FLOW RATE T%d", toolIndex);
	ConfigureStandardTunePopup(StandardPopupContext::TuneFlow, tunePopupTitleText.c_str(), true, labels, deltas, evTunePopupAdjustPercent);
	UpdateTunePopupValue();
	mgr.SetPopup(standardPopup, AutoPlace, AutoPlace);
}

static void RefreshTuneGeneralFans()
{
	// General TUNE fans are selected by their RRF fan names (see FindNamedFan), not by fan number.
	tuneGeneralFanIndices[0] = FindNamedFan("FAN_AUX");
	tuneGeneralFanIndices[1] = FindNamedFan("FAN_CHA");
	for (unsigned int i = 0; i < 2; ++i)
	{
		const int fan = tuneGeneralFanIndices[i];
		if (fan >= 0)
		{
			tuneGeneralFanText[i].printf("%d%%", tuneFanPercent[fan]);
			tuneGeneralFanButtons[i]->SetText(tuneGeneralFanText[i].c_str());
			mgr.Show(tuneGeneralFanButtons[i], true);
			mgr.Show(tuneGeneralFanLabels[i], true);
		}
		else
		{
			mgr.Show(tuneGeneralFanButtons[i], false);
			mgr.Show(tuneGeneralFanLabels[i], false);
		}
	}
}

static void RefreshTuneToolRows()
{
	OM::Tool *tools[MaxSlots] = { nullptr };
	unsigned int toolCount = 0;
	OM::IterateToolsWhile([&tools, &toolCount](OM::Tool*& tool, size_t) {
		if (toolCount < MaxSlots)
		{
			tools[toolCount++] = tool;
		}
		return toolCount < MaxSlots;
	});

	const unsigned int maxPage = (toolCount == 0) ? 0 : (toolCount - 1) / TuneToolsPerPage;
	if (tuneToolPage > maxPage)
	{
		tuneToolPage = maxPage;
	}

	for (unsigned int row = 0; row < TuneToolsPerPage; ++row)
	{
		const unsigned int toolPos = tuneToolPage * TuneToolsPerPage + row;
		if (toolPos >= toolCount || tools[toolPos] == nullptr)
		{
			mgr.Show(tuneToolNumberButtons[row], false);
			mgr.Show(tuneToolFanButtons[row], false);
			mgr.Show(tuneToolFlowButtons[row], false);
			mgr.Show(tuneToolPaButtons[row], false);
			continue;
		}

		OM::Tool * const tool = tools[toolPos];
		const int toolIndex = tool->index;
		tuneToolNumberText[row].printf("T%d", toolIndex);
		tuneToolNumberButtons[row]->SetText(tuneToolNumberText[row].c_str());
		tuneToolNumberButtons[row]->SetEvent(evNull, toolIndex);
		tuneToolNumberButtons[row]->SetBorderVisible(toolIndex == currentTool);
		mgr.Show(tuneToolNumberButtons[row], true);

		const int fan = GetToolPartFan(tool);
		if (fan >= 0 && fan < (int)TuneMaxFans)
		{
			tuneToolFanText[row].printf("%d%%", tuneFanPercent[fan]);
			tuneToolFanButtons[row]->SetText(tuneToolFanText[row].c_str());
			tuneToolFanButtons[row]->SetEvent(evTuneToolFan, toolIndex);
			mgr.Show(tuneToolFanButtons[row], true);
		}
		else
		{
			mgr.Show(tuneToolFanButtons[row], false);
		}

		const int extruder = tool->extruders.IsEmpty() ? -1 : (int)tool->extruders.LowestSetBit();
		if (extruder >= 0 && extruder < (int)TuneMaxExtruders)
		{
			tuneToolFlowText[row].printf("%d%%", tuneExtruderFactor[extruder]);
			tuneToolFlowButtons[row]->SetText(tuneToolFlowText[row].c_str());
			tuneToolFlowButtons[row]->SetEvent(evTuneToolFlow, toolIndex);
			mgr.Show(tuneToolFlowButtons[row], true);

			if (tunePressureAdvanceValid[extruder])
			{
				tuneToolPaText[row].printf("%.4f", (double)tunePressureAdvance[extruder]);
			}
			else
			{
				tuneToolPaText[row].copy("--");
			}
			tuneToolPaButtons[row]->SetText(tuneToolPaText[row].c_str());
			tuneToolPaButtons[row]->SetEvent(evTunePressureAdvance, toolIndex);
			mgr.Show(tuneToolPaButtons[row], true);
		}
		else
		{
			mgr.Show(tuneToolFlowButtons[row], false);
			mgr.Show(tuneToolPaButtons[row], false);
		}
	}

	mgr.Show(tunePageUpButton, tuneToolPage > 0);
	mgr.Show(tunePageDownButton, tuneToolPage < maxPage);
	RefreshTuneGeneralFans();
}

static void RefreshTunePage()
{
	if (tuneSpeedButton != nullptr)
	{
		tuneSpeedText.printf("%d%%", tuneSpeedPercent);
		tuneSpeedButton->SetText(tuneSpeedText.c_str());
	}

	// Z offset is cached in the object model. Only paint it when TUNE itself
	// is deliberately refreshed (tab entry, page change, or local action).
	OM::IterateAxesWhile([](OM::Axis*& axis, size_t) {
		if (axis != nullptr && axis->letter[0] == 'Z')
		{
			tuneZOffsetText.printf("%.3f", (double)axis->babystep);
			if (tuneZOffsetButton != nullptr)
			{
				tuneZOffsetButton->SetText(tuneZOffsetText.c_str());
			}
			return false;
		}
		return true;
	});

	RefreshTuneToolRows();
}

static void CreateStatusTuneTabFields(const ColourScheme& colours)
{
	mgr.SetRoot(baseRoot);
	const Colour pageBg = UTFT::fromRGB(18, 22, 28);
	const Colour tile = UTFT::fromRGB(28, 34, 43);
	const Colour text = UTFT::fromRGB(229, 232, 236);
	const Colour muted = UTFT::fromRGB(154, 164, 178);

	DisplayField::SetDefaultFont(glcd19x21);
	DisplayField::SetDefaultColours(muted, pageBg);
	// Label + tile pairs on the top row. The labels are 21 px high and their capitals are centred on the 46 px tall tiles
	// (tile top 80): label top = 80 + (46 - 21) / 2 + 1. There are 10 px between each label and its tile; the label widths are the
	// pixel widths of the texts in glcd19x21 (SPEED: 69, FAN AUX: 93, FAN CHA: 92).
	{
		const PixelNumber labelY = 93;
		const PixelNumber gap = 10;
		const PixelNumber left = TuneX(118);
		tuneRowLabelX[0] = left;								// SPEED
		tuneRowTileX[0] = left + 69 + gap;
		tuneRowLabelX[1] = tuneRowTileX[0] + tuneRowTileW + 25;	// FAN AUX
		tuneRowTileX[1] = tuneRowLabelX[1] + 93 + gap;
		tuneRowLabelX[2] = tuneRowTileX[1] + tuneRowTileW + 25;	// FAN CHA
		tuneRowTileX[2] = tuneRowLabelX[2] + 92 + gap;
		mgr.AddField(new StaticTextField(labelY, tuneRowLabelX[0], 70, TextAlignment::Left, "SPEED:"));
		tuneGeneralFanLabels[0] = new StaticTextField(labelY, tuneRowLabelX[1], 94, TextAlignment::Left, "FAN AUX:");
		tuneGeneralFanLabels[1] = new StaticTextField(labelY, tuneRowLabelX[2], 93, TextAlignment::Left, "FAN CHA:");
		mgr.AddField(tuneGeneralFanLabels[0]);
		mgr.AddField(tuneGeneralFanLabels[1]);
	}
	mgr.AddField(new StaticTextField(153, TuneX(118), TuneW(44), TextAlignment::Left, "Tool:"));
	mgr.AddField(new StaticTextField(153, TuneX(182), TuneW(114), TextAlignment::Left, "Part Cooling:"));
	mgr.AddField(new StaticTextField(153, TuneX(316), TuneW(114), TextAlignment::Left, "Flow Rate:"));
	mgr.AddField(new StaticTextField(153, TuneX(450), TuneW(119), TextAlignment::Left, "Pressure Adv.:"));
	mgr.AddField(new StaticTextField(153, TuneX(589), TuneW(99), TextAlignment::Left, "Z Offset:"));

	DisplayField::SetDefaultColours(text, tile);
	tuneSpeedText.copy("100%");
	tuneSpeedButton = new ModernTextButton(80, tuneRowTileX[0], tuneRowTileW, 46, tuneSpeedText.c_str(), evTuneSpeed, 0, glcd19x21);
	mgr.AddField(tuneSpeedButton);

	for (unsigned int i = 0; i < 2; ++i)
	{
		tuneGeneralFanText[i].copy("0%");
	}
	tuneGeneralFanButtons[0] = new ModernTextButton(80, tuneRowTileX[1], tuneRowTileW, 46, tuneGeneralFanText[0].c_str(), evTuneGeneralFan, 0, glcd19x21);
	tuneGeneralFanButtons[1] = new ModernTextButton(80, tuneRowTileX[2], tuneRowTileW, 46, tuneGeneralFanText[1].c_str(), evTuneGeneralFan, 1, glcd19x21);
	mgr.AddField(tuneGeneralFanButtons[0]);
	mgr.AddField(tuneGeneralFanButtons[1]);

	for (unsigned int row = 0; row < TuneToolsPerPage; ++row)
	{
		const PixelNumber y = 192 + row * 62;
		tuneToolNumberText[row].printf("T%d", row);
		tuneToolFanText[row].copy("0%");
		tuneToolFlowText[row].copy("100%");
		tuneToolPaText[row].copy("--");
		tuneToolNumberButtons[row] = new ModernTextButton(y, TuneX(118), TuneW(44), 56, tuneToolNumberText[row].c_str(), evNull, row, glcd19x21);
		tuneToolNumberButtons[row]->SetBorderColour(GetModernAccentColour());
		tuneToolFanButtons[row] = new ModernTextButton(y, TuneX(182), TuneW(114), 56, tuneToolFanText[row].c_str(), evTuneToolFan, row, glcd19x21);
		tuneToolFlowButtons[row] = new ModernTextButton(y, TuneX(316), TuneW(114), 56, tuneToolFlowText[row].c_str(), evTuneToolFlow, row, glcd19x21);
		tuneToolPaButtons[row] = new ModernTextButton(y, TuneX(450), TuneW(119), 56, tuneToolPaText[row].c_str(), evTunePressureAdvance, row, glcd19x21);
		mgr.AddField(tuneToolNumberButtons[row]);
		mgr.AddField(tuneToolFanButtons[row]);
		mgr.AddField(tuneToolFlowButtons[row]);
		mgr.AddField(tuneToolPaButtons[row]);
	}

	// Z offset is adjusted live using the babystep amount selected in SETTINGS.
	DisplayField::SetDefaultColours(text, tile);
	const uint32_t tuneBabystepAmountIndex = nvData.GetBabystepAmountIndex();
	tuneZPlusText.printf("+%s", babystepAmounts[tuneBabystepAmountIndex]);
	tuneZMinusText.printf("-%s", babystepAmounts[tuneBabystepAmountIndex]);
	tuneZPlusButton = new ModernTextButton(192, TuneX(589), TuneW(99), 76, tuneZPlusText.c_str(), evTuneZPlus, 0, glcd19x21);
	mgr.AddField(tuneZPlusButton);
	tuneZOffsetText.copy("0.000");
	tuneZOffsetButton = new ModernTextButton(275, TuneX(589), TuneW(99), 76, tuneZOffsetText.c_str(), evNull, 0, glcd19x21);
	mgr.AddField(tuneZOffsetButton);
	tuneZMinusButton = new ModernTextButton(358, TuneX(589), TuneW(99), 76, tuneZMinusText.c_str(), evTuneZMinus, 0, glcd19x21);
	mgr.AddField(tuneZMinusButton);

	tunePageUpButton = new ModernIconButton(192, TuneX(736), TuneW(54), 118, IconUp, evTunePageUp);
	tunePageDownButton = new ModernIconButton(316, TuneX(736), TuneW(54), 118, IconDown, evTunePageDown);
	mgr.AddField(tunePageUpButton);
	mgr.AddField(tunePageDownButton);

	statusTuneRoot = mgr.GetRoot();
	AddStatusSubTabs(statusTuneRoot);
	// Paint the modern STATUS content background before the tab/content fields.
	mgr.SetRoot(statusTuneRoot);
	DisplayField::SetDefaultColours(text, pageBg);
	mgr.AddField(new ModernTextButton(0, masterTabWidth, DisplayX - masterTabWidth, DisplayY, nullptr, evNull, 0, glcd19x21));
	statusTuneRoot = mgr.GetRoot();
	DisplayField::SetDefaultFont(DEFAULT_FONT);
	RefreshTunePage();
}
#endif

#if DISPLAY_X == 800
static void RefreshModernSettingsPage()
{
	if (settingsIpValueButton != nullptr)
	{
		settingsIpText.copy(ipAddress.c_str());
		settingsIpValueButton->SetText(settingsIpText.c_str());
	}
	if (settingsFeedrateValueButton != nullptr)
	{
		settingsFeedrateText.printf("%u", (unsigned int)nvData.GetFeedrate());
		settingsFeedrateValueButton->SetText(settingsFeedrateText.c_str());
	}
	if (settingsBabystepValueButton != nullptr)
	{
		settingsBabystepText.copy(babystepAmounts[nvData.GetBabystepAmountIndex()]);
		settingsBabystepValueButton->SetText(settingsBabystepText.c_str());
	}
	const Colour accent = GetModernAccentColour();
	if (settingsAlwaysDimButton != nullptr)
	{
		settingsAlwaysDimButton->SetBorderVisible(nvData.GetDisplayDimmerType() == DisplayDimmerType::always);
		settingsAlwaysDimButton->SetBorderColour(accent);
	}
	if (settingsHeaterCombineButton != nullptr)
	{
		settingsHeaterCombineButton->SetBorderVisible(nvData.GetHeaterCombineType() == HeaterCombineType::combined);
		settingsHeaterCombineButton->SetBorderColour(accent);
	}
}

static void RefreshStandardPopupChoiceBorders(int pending)
{
	const Colour accent = GetModernAccentColour();
	for (size_t i = 0; i < standardPopupChoiceCount; ++i)
	{
		if (standardPopupChoiceButtons[i] != nullptr)
		{
			standardPopupChoiceButtons[i]->SetBorderVisible(standardPopupChoiceValues[i] == pending);
			standardPopupChoiceButtons[i]->SetBorderColour(accent);
		}
	}
	mgr.Refresh(false);
}

static void CloseStandardPopup()
{
	mgr.ClearPopup();
	standardPopupContext = StandardPopupContext::None;
	standardPopupChoiceCount = 0;
}

static void CreateModernStandardPopup()
{
	const Colour pageBg = UTFT::fromRGB(18, 22, 28);
	const Colour tile = UTFT::fromRGB(28, 34, 43);
	const Colour text = UTFT::fromRGB(229, 232, 236);
	const Colour accent = GetModernAccentColour();
	const Colour cancelRed = UTFT::fromRGB(201, 50, 24);
	const Colour confirmGreen = UTFT::fromRGB(86, 184, 52);   // check mark fill #56B834

	standardPopup = new PopupWindow(460, 660, pageBg, accent);

	// The same title card is resized/repositioned for SHORT or LONG name-tile modes.
	DisplayField::SetDefaultFont(glcd28x32);
	DisplayField::SetDefaultColours(text, tile);
	standardPopupNameCard = new ModernCard(35, 205, 250, 60, tile, accent, false);
	standardPopupNameField = new StaticTextField(49, 205, 250, TextAlignment::Centre, "");	// y=49: capitals in glcd28x32 are centred in their 32-row cell, so this centres them in the 60 px tile (35..94)
	// Add text first and card second because AddField() prepends fields.
	// The card will therefore render first and the text will render on top.
	standardPopup->AddField(standardPopupNameField);
	standardPopup->AddField(standardPopupNameCard);

	// Reusable 550x150 information tile. It is hidden for choice popups and
	// shown only by confirmation/information contexts. Three text rows cover
	// the longest agreed SETTINGS confirmation without another allocation.
	DisplayField::SetDefaultColours(text, tile);
	standardPopupInfoCard = new ModernCard(139, 55, 550, 150, tile, UTFT::fromRGB(59, 67, 79), true);
	standardPopupInfoCard->Show(false);
	DisplayField::SetDefaultFont(glcd19x21);
	for (size_t i = 0; i < ARRAY_SIZE(standardPopupInfoFields); ++i)
	{
		standardPopupInfoFields[i] = new StaticTextField(205, 70, 520, TextAlignment::Centre, "");
		standardPopupInfoFields[i]->Show(false);
		standardPopup->AddField(standardPopupInfoFields[i]);
	}
	// Add the card after the text fields so it is prepended ahead of them
	// and therefore renders first, leaving the text visible on top.
	standardPopup->AddField(standardPopupInfoCard);

	// Reusable middle-area button pool. Only the buttons required by the current
	// popup context are shown. Sixteen covers 10 RRF M291 axis selectors plus 6 jog buttons;
	// ordinary M291 Choices still uses only the first 10 slots.
	DisplayField::SetDefaultFont(glcd19x21);
	for (size_t i = 0; i < ARRAY_SIZE(standardPopupChoiceButtons); ++i)
	{
		DisplayField::SetDefaultColours(text, tile);
		standardPopupChoiceButtons[i] = new ModernTextButton(145, 0, 100, 60, "", evStandardPopupChoice, 0, glcd19x21, false);
		standardPopupChoiceButtons[i]->Show(false);
		standardPopup->AddField(standardPopupChoiceButtons[i]);
	}

	// Bottom actions use the final fixed 110x78 geometry and y=332 position.
	// Trash is normally hidden and is only shown by the JOB file-detail context.
	DisplayField::SetDefaultColours(UTFT::fromRGB(36,36,36), UTFT::fromRGB(95,195,220));
	standardPopupTrashButton = new ModernIconButton(332, 102, 110, 78, IconTrash, evStatusJobDeleteOpen);
	standardPopupTrashButton->Show(false);
	standardPopup->AddField(standardPopupTrashButton);
	DisplayField::SetDefaultColours(UTFT::fromRGB(36,36,36), cancelRed);
	standardPopupCancelButton = new ModernIconButton(332, 189, 110, 78, IconCancel, evStandardPopupCancel);
	standardPopup->AddField(standardPopupCancelButton);
	DisplayField::SetDefaultColours(UTFT::fromRGB(36,36,36), confirmGreen);
	standardPopupConfirmButton = new ModernIconButton(332, 362, 110, 78, IconOk, evStandardPopupConfirm);
	standardPopup->AddField(standardPopupConfirmButton);
	DisplayField::SetDefaultFont(DEFAULT_FONT);
}

static void ConfigureStandardPopupTitle(const char *title, bool longTitle)
{
	const PixelNumber titleX = longTitle ? 55 : 205;
	const PixelNumber titleW = longTitle ? 550 : 250;
	standardPopupNameCard->SetPosition(titleX, 35);
	standardPopupNameCard->SetPositionAndWidth(titleX, titleW);
	standardPopupNameField->SetPosition(titleX, 49);
	standardPopupNameField->SetPositionAndWidth(titleX, titleW);
	standardPopupNameField->SetValue(title, true);
}

static void HideStandardPopupChoices()
{
	for (size_t i = 0; i < ARRAY_SIZE(standardPopupChoiceButtons); ++i)
	{
		standardPopupChoiceButtons[i]->Show(false);
		standardPopupChoiceButtons[i]->SetEvent(evNull, 0);
		standardPopupChoiceButtons[i]->SetBorderVisible(false);	// no stale selection/value border from a previous context
		standardPopupChoiceValues[i] = 0;
	}
	standardPopupChoiceCount = 0;
}

static void HideStandardPopupInformation()
{
	standardPopupInfoCard->Show(false);
	for (size_t i = 0; i < ARRAY_SIZE(standardPopupInfoFields); ++i)
	{
		standardPopupInfoFields[i]->Show(false);
		standardPopupInfoFields[i]->SetValue("", true);
	}
}

static void ResetStandardPopupContent()
{
	HideStandardPopupChoices();
	HideStandardPopupInformation();
	standardPopupNameCard->Show(true);
	standardPopupNameField->Show(true);
	// Default two-action layout. JOB START temporarily switches to three actions.
	standardPopupTrashButton->Show(false);
	standardPopupCancelButton->SetPosition(189, 332);
	standardPopupConfirmButton->SetPosition(362, 332);
}

static void ConfigureStandardPopupInformation(const char *line1, const char *line2, const char *line3, const char *line4)
{
	HideStandardPopupInformation();
	standardPopupInfoCard->Show(true);

	if (line4 != nullptr)
	{
		const PixelNumber ypos[4] = {158, 190, 222, 254};
		const char * const lines[4] = {line1, line2, line3, line4};
		for (size_t i = 0; i < 4; ++i)
		{
			standardPopupInfoFields[i]->SetPosition(70, ypos[i]);
			standardPopupInfoFields[i]->SetValue(lines[i], true);
			standardPopupInfoFields[i]->Show(true);
		}
	}
	else if (line3 != nullptr)
	{
		const PixelNumber ypos[3] = {170, 205, 240};
		const char * const lines[3] = {line1, line2, line3};
		for (size_t i = 0; i < 3; ++i)
		{
			standardPopupInfoFields[i]->SetPosition(70, ypos[i]);
			standardPopupInfoFields[i]->SetValue(lines[i], true);
			standardPopupInfoFields[i]->Show(true);
		}
	}
	else if (line2 != nullptr)
	{
		const PixelNumber ypos[2] = {186, 226};
		const char * const lines[2] = {line1, line2};
		for (size_t i = 0; i < 2; ++i)
		{
			standardPopupInfoFields[i]->SetPosition(70, ypos[i]);
			standardPopupInfoFields[i]->SetValue(lines[i], true);
			standardPopupInfoFields[i]->Show(true);
		}
	}
	else
	{
		standardPopupInfoFields[0]->SetPosition(70, 205);
		standardPopupInfoFields[0]->SetValue(line1, true);
		standardPopupInfoFields[0]->Show(true);
	}
}

static void CreateModernSettingsPopups()
{
	// Shared standard popup is constructed once and reused by all modern SETTINGS popups.
	CreateModernStandardPopup();
}

static void OpenSettingsVolumePopup()
{
	static const char * const labels[6] = {"0","1","2","3","4","5"};
	static const int values[6] = {0,1,2,3,4,5};
	const Colour tile = UTFT::fromRGB(28, 34, 43);
	const Colour text = UTFT::fromRGB(229, 232, 236);

	standardPopupContext = StandardPopupContext::SettingsVolume;
	ConfigureStandardPopupTitle("VOLUME", false);
	ResetStandardPopupContent();
	standardPopupChoiceCount = 6;
	settingsPendingVolume = (int)nvData.GetVolume();

	for (size_t i = 0; i < standardPopupChoiceCount; ++i)
	{
		const PixelNumber x = 150 + (i % 3) * 130;
		const PixelNumber y = 145 + (i / 3) * 75;
		ModernTextButton * const button = standardPopupChoiceButtons[i];
		button->SetPosition(x, y);
		button->SetPositionAndWidth(x, 100);
		button->SetColours(text, tile);
		button->SetText(labels[i]);
		button->SetEvent(evStandardPopupChoice, values[i]);
		standardPopupChoiceValues[i] = values[i];
		button->Show(true);
	}

	standardPopupCancelButton->Show(true);
	standardPopupConfirmButton->Show(true);
	RefreshStandardPopupChoiceBorders(settingsPendingVolume);
	mgr.SetPopup(standardPopup, AutoPlace, AutoPlace);
}
static void OpenSettingsBrightnessPopup()
{
	static const char * const labels[6] = {"0%","20%","40%","60%","80%","100%"};
	static const int values[6] = {0,20,40,60,80,100};
	const Colour tile = UTFT::fromRGB(28, 34, 43);
	const Colour text = UTFT::fromRGB(229, 232, 236);

	standardPopupContext = StandardPopupContext::SettingsBrightness;
	ConfigureStandardPopupTitle("BRIGHTNESS", false);
	ResetStandardPopupContent();
	standardPopupChoiceCount = 6;
	settingsPendingBrightness = nvData.GetBrightness();

	for (size_t i = 0; i < standardPopupChoiceCount; ++i)
	{
		const PixelNumber x = 150 + (i % 3) * 130;
		const PixelNumber y = 145 + (i / 3) * 75;
		ModernTextButton * const button = standardPopupChoiceButtons[i];
		button->SetPosition(x, y);
		button->SetPositionAndWidth(x, 100);
		button->SetColours(text, tile);
		button->SetText(labels[i]);
		button->SetEvent(evStandardPopupChoice, values[i]);
		standardPopupChoiceValues[i] = values[i];
		button->Show(true);
	}

	standardPopupCancelButton->Show(true);
	standardPopupConfirmButton->Show(true);
	RefreshStandardPopupChoiceBorders(settingsPendingBrightness);
	mgr.SetPopup(standardPopup, AutoPlace, AutoPlace);
}
static void OpenSettingsInfoTimeoutPopup()
{
	static const char * const labels[4] = {"0s","3s","6s","10s"};
	static const int values[4] = {0,3,6,10};
	const Colour tile = UTFT::fromRGB(28, 34, 43);
	const Colour text = UTFT::fromRGB(229, 232, 236);

	standardPopupContext = StandardPopupContext::SettingsInfoTimeout;
	ConfigureStandardPopupTitle("INFO TIMEOUT", false);
	ResetStandardPopupContent();
	standardPopupChoiceCount = 4;
	settingsPendingInfoTimeout = (int)infoTimeout;

	for (size_t i = 0; i < standardPopupChoiceCount; ++i)
	{
		const PixelNumber x = 100 + i * 120;
		const PixelNumber y = 185;
		ModernTextButton * const button = standardPopupChoiceButtons[i];
		button->SetPosition(x, y);
		button->SetPositionAndWidth(x, 100);
		button->SetColours(text, tile);
		button->SetText(labels[i]);
		button->SetEvent(evStandardPopupChoice, values[i]);
		standardPopupChoiceValues[i] = values[i];
		button->Show(true);
	}

	standardPopupCancelButton->Show(true);
	standardPopupConfirmButton->Show(true);
	RefreshStandardPopupChoiceBorders(settingsPendingInfoTimeout);
	mgr.SetPopup(standardPopup, AutoPlace, AutoPlace);
}
static void OpenSettingsAccentPopup()
{
	static const int values[8] = {0,1,2,3,4,5,6,7};

	standardPopupContext = StandardPopupContext::SettingsAccent;
	ConfigureStandardPopupTitle("ACCENT COLOR", true);
	ResetStandardPopupContent();
	standardPopupChoiceCount = 8;
	settingsPendingAccent = nvData.GetAccentColour();

	// 4x2 SHORT swatches. Reuse the standard choice-button pool, but make each
	// button a solid fill of the Accent colour it represents and draw no label.
	for (size_t i = 0; i < standardPopupChoiceCount; ++i)
	{
		const PixelNumber x = 100 + (i % 4) * 120;
		const PixelNumber y = 140 + (i / 4) * 80;
		const Colour swatchColour = GetModernAccentColourByIndex((uint8_t)i);
		ModernTextButton * const button = standardPopupChoiceButtons[i];
		button->SetPosition(x, y);
		button->SetPositionAndWidth(x, 100);
		button->SetColours(swatchColour, swatchColour);
		button->SetText("");
		button->SetEvent(evStandardPopupChoice, values[i]);
		standardPopupChoiceValues[i] = values[i];
		button->Show(true);
	}

	standardPopupCancelButton->Show(true);
	standardPopupConfirmButton->Show(true);
	RefreshStandardPopupChoiceBorders(settingsPendingAccent);
	mgr.SetPopup(standardPopup, AutoPlace, AutoPlace);
}
static void OpenSettingsBaudPopup()
{
	static const char * const labels[5] = {"9600","19200","38400","57600","115200"};
	static const int values[5] = {9600,19200,38400,57600,115200};
	static const PixelNumber xpos[5] = {90,255,420,170,340};
	static const PixelNumber ypos[5] = {145,145,145,220,220};
	const Colour tile = UTFT::fromRGB(28, 34, 43);
	const Colour text = UTFT::fromRGB(229, 232, 236);

	standardPopupContext = StandardPopupContext::SettingsBaud;
	ConfigureStandardPopupTitle("BAUD LINK", false);
	ResetStandardPopupContent();
	standardPopupChoiceCount = 5;
	settingsPendingBaud = (int)nvData.GetBaudRate();

	for (size_t i = 0; i < standardPopupChoiceCount; ++i)
	{
		ModernTextButton * const button = standardPopupChoiceButtons[i];
		button->SetPosition(xpos[i], ypos[i]);
		button->SetPositionAndWidth(xpos[i], 150);
		button->SetColours(text, tile);
		button->SetText(labels[i]);
		button->SetEvent(evStandardPopupChoice, values[i]);
		standardPopupChoiceValues[i] = values[i];
		button->Show(true);
	}

	standardPopupCancelButton->Show(true);
	standardPopupConfirmButton->Show(true);
	RefreshStandardPopupChoiceBorders(settingsPendingBaud);
	mgr.SetPopup(standardPopup, AutoPlace, AutoPlace);
}

static void OpenSettingsFeedratePopup()
{
	static const char * const labels[5] = {"600","1200","2400","6000","12000"};
	static const int values[5] = {600,1200,2400,6000,12000};
	static const PixelNumber xpos[5] = {90,255,420,170,340};
	static const PixelNumber ypos[5] = {145,145,145,220,220};
	const Colour tile = UTFT::fromRGB(28, 34, 43);
	const Colour text = UTFT::fromRGB(229, 232, 236);

	standardPopupContext = StandardPopupContext::SettingsFeedrate;
	ConfigureStandardPopupTitle("FEEDRATE", false);
	ResetStandardPopupContent();
	standardPopupChoiceCount = 5;
	settingsPendingFeedrate = (int)nvData.GetFeedrate();

	for (size_t i = 0; i < standardPopupChoiceCount; ++i)
	{
		ModernTextButton * const button = standardPopupChoiceButtons[i];
		button->SetPosition(xpos[i], ypos[i]);
		button->SetPositionAndWidth(xpos[i], 150);
		button->SetColours(text, tile);
		button->SetText(labels[i]);
		button->SetEvent(evStandardPopupChoice, values[i]);
		standardPopupChoiceValues[i] = values[i];
		button->Show(true);
	}

	standardPopupCancelButton->Show(true);
	standardPopupConfirmButton->Show(true);
	RefreshStandardPopupChoiceBorders(settingsPendingFeedrate);
	mgr.SetPopup(standardPopup, AutoPlace, AutoPlace);
}

static void OpenSettingsBabystepPopup()
{
	static const int values[4] = {0,1,2,3};
	static const PixelNumber xpos[4] = {87,216,345,474};
	const Colour tile = UTFT::fromRGB(28, 34, 43);
	const Colour text = UTFT::fromRGB(229, 232, 236);

	standardPopupContext = StandardPopupContext::SettingsBabystep;
	ConfigureStandardPopupTitle("Z OFFSET STEP", true);
	ResetStandardPopupContent();
	standardPopupChoiceCount = 4;
	settingsPendingBabystepIndex = (int)nvData.GetBabystepAmountIndex();

	for (size_t i = 0; i < standardPopupChoiceCount; ++i)
	{
		ModernTextButton * const button = standardPopupChoiceButtons[i];
		button->SetPosition(xpos[i], 184);
		button->SetPositionAndWidth(xpos[i], 100);
		button->SetColours(text, tile);
		button->SetText(babystepAmounts[i]);
		button->SetEvent(evStandardPopupChoice, values[i]);
		standardPopupChoiceValues[i] = values[i];
		button->Show(true);
	}

	standardPopupCancelButton->Show(true);
	standardPopupConfirmButton->Show(true);
	RefreshStandardPopupChoiceBorders(settingsPendingBabystepIndex);
	mgr.SetPopup(standardPopup, AutoPlace, AutoPlace);
}

static void OpenSettingsTouchCalibrationPopup()
{
	standardPopupContext = StandardPopupContext::SettingsTouchCalibration;
	ConfigureStandardPopupTitle("TOUCH CALIBRATION", true);
	ResetStandardPopupContent();
	ConfigureStandardPopupInformation("Do you want to perform touch calibration?");
	standardPopupCancelButton->Show(true);
	standardPopupConfirmButton->Show(true);
	mgr.SetPopup(standardPopup, AutoPlace, AutoPlace);
}

static void OpenSettingsHeaterCombinationPopup()
{
	standardPopupContext = StandardPopupContext::SettingsHeaterCombination;
	ConfigureStandardPopupTitle("HEATER COMBINATION", true);
	ResetStandardPopupContent();
	ConfigureStandardPopupInformation("Combine multiple heaters assigned to the same tool", "into a single temperature control.");
	standardPopupCancelButton->Show(true);
	standardPopupConfirmButton->Show(true);
	mgr.SetPopup(standardPopup, AutoPlace, AutoPlace);
}

static void OpenSettingsFactoryResetPopup()
{
	standardPopupContext = StandardPopupContext::SettingsFactoryReset;
	ConfigureStandardPopupTitle("ALERT !", false);
	ResetStandardPopupContent();
	ConfigureStandardPopupInformation("FACTORY RESET will return", "Display to default values", "Continue?");
	standardPopupCancelButton->Show(true);
	standardPopupConfirmButton->Show(true);
	mgr.SetPopup(standardPopup, AutoPlace, AutoPlace);
}
#endif

static void CreateMainPages(const ColourScheme& colours)
{
	emptyRoot = mgr.GetRoot();
	mgr.SetLeftMargin(masterTabWidth);
	strings = &LanguageTables[0];
	CreateCommonFields(colours);
	baseRoot = mgr.GetRoot();		// save the root of fields that we usually display

	// Create the fields that are common to the Control and Print pages
	DisplayField::SetDefaultColours(colours.titleBarTextColour, colours.titleBarBackColour);
	mgr.AddField(nameField = new StaticTextField(row1, 0, DisplayX - statusFieldWidth, TextAlignment::Centre, machineName.c_str()));
	mgr.AddField(statusField = new StaticTextField(row1, DisplayX - statusFieldWidth, statusFieldWidth, TextAlignment::Right, nullptr));
	CreateTemperatureGrid(colours);
	commonRoot = mgr.GetRoot();		// save the root of fields that we display on more than one page

	// Create the pages
	CreateControlTabFields(colours);
	CreatePrintingTabFields(colours);
	CreateStatusObjectsTabFields(colours);
	CreateMessageTabFields(colours);
	CreateSetupTabFields(colours);

	RelayoutLegacyFields();
	AddControlSubTabs();
	AddStatusSubTabs(printRoot);
#if DISPLAY_X == 800
	CreateControlToolsTabFields(colours);
	CreateControlMovementTabFields(colours);
	CreateControlExtrusionTabFields(colours);
	CreateControlMacrosTabFields(colours);
	CreateStatusJobStatusTabFields(colours);
	CreateStatusTuneTabFields(colours);
	CreateStatusJobTabFields(colours);
#endif
#if DISPLAY_X != 800
	AddStatusSubTabs(statusObjectsRoot);
#endif
	AddSystemSubTabs(messageRoot);
	AddSystemSubTabs(setupRoot);
#if DISPLAY_X == 800
	// Paint the native SETTINGS content background after the fields/subtabs so it
	// becomes the root field and renders underneath them.
	mgr.SetRoot(setupRoot);
	const Colour settingsPageBg = UTFT::fromRGB(18, 22, 28);
	mgr.AddField(new ModernCard(contentTop, masterTabWidth, DisplayX - masterTabWidth,
		DisplayY - contentTop, settingsPageBg, settingsPageBg, false));
	setupRoot = mgr.GetRoot();

	// Paint the Console content pane with the same dark page background used by
	// the modern CONTROL/STATUS pages. Add it after the SYSTEM tabs so it becomes
	// the root field and therefore paints first, underneath the log and controls.
	mgr.SetRoot(messageRoot);
	const Colour consolePageBg = UTFT::fromRGB(18, 22, 28); // #12161c
	mgr.AddField(new ModernCard(contentTop, masterTabWidth, DisplayX - masterTabWidth,
		DisplayY - contentTop, consolePageBg, consolePageBg, false));
	messageRoot = mgr.GetRoot();


	CreateModernSettingsPopups();
	RefreshModernSettingsPage();
#endif
#if DISPLAY_X != 800
	CreateScreensaverPopup();
#endif
	CreateFirmwareUpdatePopup();
}

namespace UI
{
	static void Adjusting(ButtonPress bp)
	{
		fieldBeingAdjusted = bp;
		if (bp == currentButton)
		{
			currentButton.Clear();		// to stop it being released
		}
	}

	static void StopAdjusting()
	{
		if (fieldBeingAdjusted.IsValid())
		{
			mgr.Press(fieldBeingAdjusted, false);
			fieldBeingAdjusted.Clear();
		}
	}

	static void CurrentButtonReleased()
	{
		if (currentButton.IsValid())
		{
			mgr.Press(currentButton, false);
			currentButton.Clear();
		}
	}

	static void ClearAlertOrResponse();

	void InitColourScheme(const ColourScheme *scheme)
	{
		colours = scheme;
	}

	// Create all the fields we ever display
	void CreateFields(uint32_t language, const ColourScheme& colours, uint32_t p_infoTimeout)
	{
		UNUSED(language);		// modern UI uses a single language table
		infoTimeout = p_infoTimeout;

		// Set up default colours and margins
#if DISPLAY_X == 800
		// The modern UI is always dark. Window::Show(false)/Redraw() erase hidden fields with the main window's
		// background colour, and the default colour scheme's is white, which left white tiles behind hidden
		// buttons (PAUSE/ABORT, list arrows and unused list rows). Use the page background instead.
		mgr.Init(UTFT::fromRGB(18, 22, 28));		// #12161c
#else
		mgr.Init(colours.defaultBackColour);
#endif
		DisplayField::SetDefaultFont(DEFAULT_FONT);
		ButtonWithText::SetFont(DEFAULT_FONT);
		CharButtonRow::SetFont(DEFAULT_FONT);
		SingleButton::SetTextMargin(textButtonMargin);
		SingleButton::SetIconMargin(iconButtonMargin);

		// Create the pages
		CreateMainPages(colours);

		// Create the popup fields
#if DISPLAY_X != 800
		CreateIntegerAdjustPopup(colours);
		CreateIntegerRPMAdjustPopup(colours);
#endif
		CreateMovePopup(colours);
#if DISPLAY_X != 800
		CreateExtrudePopup(colours);
#endif
		fileListPopup = CreateFileListPopup(filesListButtons, filenameButtons, NumFileRows, NumFileColumns, colours, true);
		macrosPopup = CreateFileListPopup(macrosListButtons, macroButtons, NumMacroRows, NumMacroColumns, colours, false);
#if DISPLAY_X != 800
		CreateFileActionPopup(colours);
#endif
#if DISPLAY_X != 800
		CreateVolumePopup(colours);
#endif
#if DISPLAY_X != 800
		CreateInfoTimeoutPopup(colours);
#endif
#if DISPLAY_X != 800
		CreateScreensaverTimeoutPopup(colours);
#endif
#if DISPLAY_X != 800
		CreateBabystepAmountPopup(colours);
		CreateFeedrateAmountPopup(colours);
#endif
#if DISPLAY_X != 800
		CreateBaudRatePopup(colours);
#endif
#if DISPLAY_X != 800
		CreateColoursPopup(colours);
#endif
#if DISPLAY_X != 800
		CreateAreYouSurePopup(colours);
#endif
		CreateKeyboardPopup(colours);
#if DISPLAY_X != 800
		alertPopup = new AlertPopup(colours);
#endif
#if DISPLAY_X != 800
		CreateBabystepPopup(colours);
#endif

		DisplayField::SetDefaultColours(colours.labelTextColour, colours.defaultBackColour);
		touchCalibInstruction = new StaticTextField(DisplayY/2 - 10, 0, DisplayX, TextAlignment::Centre, strings->touchTheSpot);

		mgr.SetRoot(nullptr);

#ifdef SUPPORT_ENCODER
		encoder = new RotaryEncoder(2, 3, 32+6);			// PA2, PA3 and PB6
		encoder->Init(4);
#endif
	}

	// This is called when no job is active/paused
	void ShowFilesButton()
	{
		// First hide everything removed then show everything new
		// otherwise remnants of the to-be-hidden might remain
		mgr.Show(resumeButton,		false);
		mgr.Show(cancelButton,		false);
		mgr.Show(pauseButton,		false);
		mgr.Show(printProgressBar,	false);

		mgr.Show(babystepButton,	true);
		mgr.Show(reprintButton,		lastJobFileNameAvailable);
		mgr.Show(filesButton,		true);
	}

	// This is called when a job is active
	void ShowPauseButton()
	{
		// First hide everything removed then show everything new
		// otherwise remnants of the to-be-hidden might remain
		mgr.Show(resumeButton,		false);
		mgr.Show(cancelButton,		false);
		mgr.Show(filesButton,		false);
		mgr.Show(reprintButton,		false);

		mgr.Show(pauseButton,		true);
		mgr.Show(babystepButton,	true);
		mgr.Show(printProgressBar,	true);
	}

	// This is called when a job is paused
	void ShowResumeAndCancelButtons()
	{
		// First hide everything removed then show everything new
		// otherwise remnants of the to-be-hidden might remain
		mgr.Show(filesButton,		false);
		mgr.Show(pauseButton,		false);
		mgr.Show(reprintButton,		false);
		mgr.Show(babystepButton,	false);

		mgr.Show(cancelButton,		true);
		mgr.Show(resumeButton,		true);
		mgr.Show(printProgressBar,	true);
	}

	// Show or hide an axis on the move button grid and on the axis display
	void ShowAxis(size_t slot, bool b, const char* axisLetter)
	{
		if (slot >= MaxDisplayableAxes)
		{
			return;
		}
		// The table gives us a pointer to the label field, which is followed by 8 buttons. So we need to show or hide 9 fields.
		DisplayField *f = moveAxisRows[slot];
		for (int i = 0; i < 9 && f != nullptr; ++i)
		{
			mgr.Show(f, b);
			if (i > 0) // actual move buttons
			{
				TextButtonForAxis *textButton = static_cast<TextButtonForAxis*>(f);
				textButton->SetAxisLetter(axisLetter[0]);
			}
			f = f->next;
		}
		mgr.Show(controlTabAxisPos[slot], b);
#if DISPLAY_X == 800
		mgr.Show(printTabAxisPos[slot], b);
#endif
		if (numDisplayedAxes < MaxDisplayableAxes)
		{
			mgr.Show(movePopupAxisPos[slot], b);		// the move popup axis positions occupy the last axis row of the move popup
		}
		else
		{
			// This is incremental and we might end up that this row is no longer available
			for (size_t i = 0; i < MaxDisplayableAxes; ++i)
			{
				mgr.Show(movePopupAxisPos[i], false);
			}
		}
	}

	void UpdateAxisPosition(size_t axisIndex, float fval)
	{
		if (axisIndex < MaxTotalAxes)
		{
			auto axis = OM::GetAxis(axisIndex);
			if (axis != nullptr)
			{
				#if DISPLAY_X == 800
				const int moveAxisSlot = GetControlMoveAxisSlot(axis->letter[0]);
				if (moveAxisSlot >= 0)
				{
					controlMovePosition[moveAxisSlot] = fval;
					controlMovePositionValid[moveAxisSlot] = true;
					RefreshControlMovePosition(static_cast<unsigned int>(moveAxisSlot));
				}
				#endif
				if (axis->slot >= MaxDisplayableAxes)
				{
					return;
				}
				size_t slot = axis->slot;

				if (axisMaxVal > 1000)
				{
					controlTabAxisPos[slot]->SetNumDecimals(1);
#if DISPLAY_X == 800
					printTabAxisPos[slot]->SetNumDecimals(1);
#endif
					if (movePopupAxisPos[slot] != nullptr)
					{
						movePopupAxisPos[slot]->SetNumDecimals(1);
					}
				}

				controlTabAxisPos[slot]->SetValue(fval);
#if DISPLAY_X == 800
				printTabAxisPos[slot]->SetValue(fval);
#endif
				if (movePopupAxisPos[slot] != nullptr)
				{
					movePopupAxisPos[slot]->SetValue(fval);
				}
			}
		}
	}

	void UpdateCurrentTemperature(size_t heaterIndex, float fval)
	{
#if DISPLAY_X == 800
		if (heaterIndex < JobStatusMaxHeaters)
		{
			jobStatusHeaterTemps[heaterIndex] = fval;
			jobStatusHeaterValid[heaterIndex] = true;
			if (currentUiPage == UiPage::ControlExtrusion)
			{
				RefreshControlExtrudeTools();
			}
			if (currentUiPage == UiPage::StatusJobStatus)
			{
				RefreshJobStatusTilesByType(JobStatusTileType::ToolTemp);
				RefreshJobStatusTilesByType(JobStatusTileType::BedTemp);
				RefreshJobStatusTilesByType(JobStatusTileType::ChamberTemp);
			}
		}
#endif
		OM::Slots heaterSlots;
		OM::GetHeaterSlots(heaterIndex, heaterSlots);
		if (!heaterSlots.IsEmpty())
		{
			const size_t count = heaterSlots.Size();
			for (size_t i = 0; i < count; ++i)
			{
				currentTemps[heaterSlots[i]]->SetValue(fval);
			}

			heaterSlots.Clear();
		}
	}

	void UpdateHeaterStatus(const size_t heaterIndex, const OM::HeaterStatus status)
	{
#if DISPLAY_X == 800
		if (heaterIndex < JobStatusMaxHeaters)
		{
			const bool statusChanged = (jobStatusHeaterStatus[heaterIndex] != status);
			jobStatusHeaterStatus[heaterIndex] = status;
			if (statusChanged && currentUiPage == UiPage::ControlTools)
			{
				RefreshControlToolsPage();
			}
			if (currentUiPage == UiPage::StatusJobStatus)
			{
				RefreshJobStatusTilesByType(JobStatusTileType::ToolTemp);
				RefreshJobStatusTilesByType(JobStatusTileType::BedTemp);
				RefreshJobStatusTilesByType(JobStatusTileType::ChamberTemp);
			}
		}
#endif
		OM::Slots heaterSlots;
		OM::GetHeaterSlots(heaterIndex, heaterSlots);
		const Colour foregroundColour =	(status == OM::HeaterStatus::fault)
					? colours->errorTextColour
					: colours->infoTextColour;
		const Colour backgroundColour =
					  (status == OM::HeaterStatus::standby) ? colours->standbyBackColour
					: (status == OM::HeaterStatus::active)  ? colours->activeBackColour
					: (status == OM::HeaterStatus::fault)   ? colours->errorBackColour
					: (status == OM::HeaterStatus::tuning)  ? colours->tuningBackColour
					: colours->defaultBackColour;
		if (!heaterSlots.IsEmpty())
		{
			const Colour bedOrChamberBgColor = (backgroundColour == colours->defaultBackColour)
				? colours->buttonImageBackColour
				: backgroundColour;
			const size_t count = heaterSlots.Size();
			for (size_t i = 0; i < count; ++i)
			{
				const size_t slot = heaterSlots[i];
				currentTemps[slot]->SetColours(foregroundColour, backgroundColour);

				// If it's a bed or a chamber we update colors for the tool button as well
				OM::IterateBedsWhile([&heaterIndex, &status, &foregroundColour, &bedOrChamberBgColor, &slot](OM::Bed*& bed, size_t) {
					if (bed->heater == (int)heaterIndex)
					{
						bed->heaterStatus = status;
						toolButtons[slot]->SetColours(foregroundColour, bedOrChamberBgColor);
						return false;	// This will lead to getting out of the iteration on first hit - is that really what we want?
					}
					return true;
				});
				OM::IterateChambersWhile([&heaterIndex, &status, &foregroundColour, &bedOrChamberBgColor, &slot](OM::Chamber*& chamber, size_t) {
					if (chamber->heater == (int)heaterIndex)
					{
						chamber->heaterStatus = status;
						toolButtons[slot]->SetColours(foregroundColour, bedOrChamberBgColor);
						return false;	// This will lead to getting out of the iteration on first hit - is that really what we want?
					}
					return true;
				});
			}
			heaterSlots.Clear();
		}
	}

	void SetCurrentTool(int32_t ival)
	{
		if (ival == currentTool)
		{
			return;
		}
		currentTool = ival;
#if DISPLAY_X == 800
		if (jobStatusLabels[0] != nullptr)
		{
			RefreshJobStatusTiles();
		}
		if (controlToolHeaderCards[0] != nullptr)
		{
			RefreshControlToolsPage();
		}
		if (controlExtrudeActiveToolCard != nullptr)
		{
			SelectControlExtrudePageForActiveTool();
			RefreshControlExtrudeTools();
		}
#endif
	}

	enum TimesLeft { file, filament, slicer, max };
	static int timesLeft[TimesLeft::max];
	static uint32_t simulatedTime;
	static uint32_t jobDuration;
	static uint32_t jobWarmUpDuration;
	static String<50> timesLeftText;

	static const char *GetStatusString(OM::PrinterStatus status)
	{
		unsigned int index = (unsigned int)status;
		if (index >= ARRAY_SIZE(strings->statusValues) || !strings->statusValues[index])
		{
			return "unknown status";
		}

		return strings->statusValues[index];
	}

	void ChangeStatus(OM::PrinterStatus oldStatus, OM::PrinterStatus newStatus)
	{
#if DISPLAY_X == 800
		if (jobAbortWhenPaused)
		{
			if (newStatus == OM::PrinterStatus::paused)
			{
				jobAbortWhenPaused = false;
				SerialIo::Sendf("M0\n");			// the print is paused now, so it can be cancelled
			}
			else if (newStatus != OM::PrinterStatus::printing && newStatus != OM::PrinterStatus::pausing &&
					 newStatus != OM::PrinterStatus::resuming && newStatus != OM::PrinterStatus::simulating)
			{
				jobAbortWhenPaused = false;			// the job ended (or something else happened) before it paused
			}
		}
#endif

		if (oldStatus != newStatus)
		{
			const char *fromStatus = GetStatusString(oldStatus);
			const char *toStatus = GetStatusString(newStatus);

			MessageLog::AppendMessageF(MessageLog::LogLevel::Verbose,
					"Info: status changed from %s to %s.", fromStatus, toStatus);
		}

		switch (newStatus)
		{
		case OM::PrinterStatus::printing:
		case OM::PrinterStatus::simulating:
			if (oldStatus != OM::PrinterStatus::paused && oldStatus != OM::PrinterStatus::resuming)
			{
				// Starting a new print, so clear the times
				timesLeft[0] = timesLeft[1] = timesLeft[2] = 0;
				simulatedTime = 0;
#if DISPLAY_X == 800
				jobStatusLayer = jobStatusNumLayers = jobStatusProgress = 0;
				jobStatusDuration = 0;
#endif
			}
			SetLastFileSimulated(newStatus == OM::PrinterStatus::simulating);
			if (oldStatus != newStatus)
			{
				PrintStarted();
			}
			[[fallthrough]];
		case OM::PrinterStatus::paused:
		case OM::PrinterStatus::pausing:
		case OM::PrinterStatus::resuming:
			if (currentTab == tabStatus)
			{
				nameField->SetValue(printingFile.c_str());
			}
			break;

		case OM::PrinterStatus::idle:
			printingFile.Clear();
#if DISPLAY_X == 800
			jobStatusLayer = jobStatusNumLayers = jobStatusProgress = 0;
			jobStatusDuration = 0;
			UpdateStatusObjectCount(0);
			currentStatusObject = -1;
			selectedStatusObject = -1;
			statusObjectPage = 0;
#endif
			nameField->SetValue(machineName.c_str());		// if we are on the print tab then it may still be set to the file that was being printed
			if (IsPrintingStatus(oldStatus))
			{
				mgr.Refresh(true);		// Ending a print creates a popup and that will prevent removing some of the elements hidden so force it here
			}
			[[fallthrough]];
		case OM::PrinterStatus::configuring:
			if (oldStatus == OM::PrinterStatus::flashing)
			{
				mgr.ClearAllPopups();						// clear the firmware update message
			}
			break;

		case OM::PrinterStatus::connecting:
			printingFile.Clear();
			currentFile = nullptr;		// all popups are cleared below, so no file popup can still be showing this
#if DISPLAY_X == 800
			jobStatusLayer = jobStatusNumLayers = jobStatusProgress = 0;
			jobStatusDuration = 0;
			UpdateStatusObjectCount(0);
			currentStatusObject = -1;
			selectedStatusObject = -1;
			statusObjectPage = 0;
#endif
			mgr.ClearAllPopups();
			break;

		default:
			nameField->SetValue(machineName.c_str());
			break;
		}
#if DISPLAY_X == 800
		if (controlToolHeaderCards[0] != nullptr) RefreshControlToolsPage();
		RefreshJobStatusActions();
		RefreshJobStatusHeader();
		if (newStatus == OM::PrinterStatus::printing || newStatus == OM::PrinterStatus::simulating)
		{
			if (jobStatusThumbnail != nullptr) jobStatusThumbnail->SetChanged();
		}
#endif
	}

	// Append an amount of time to timesLeftText
	static void AppendTimeLeft(int t)
	{
		if (t <= 0)
		{
			timesLeftText.cat(strings->notAvailable);
		}
		else if (t < 60)
		{
			timesLeftText.catf("%ds", t);
		}
		else if (t < 60 * 60)
		{
			timesLeftText.catf("%dm %02ds", t/60, t%60);
		}
		else
		{
			t /= 60;
			timesLeftText.catf("%dh %02dm", t/60, t%60);
		}
	}

	void UpdateTimesLeftText()
	{
		if (!PrintInProgress())
		{
			return;
		}
		size_t count = 0;
		timesLeftText.Clear();
		if (simulatedTime > 0)
		{
			timesLeftText.copy(strings->simulated);
			AppendTimeLeft(simulatedTime + jobWarmUpDuration - jobDuration);
			++count;
		}
		if (timesLeft[TimesLeft::slicer] > 0)
		{
			if (count > 0) {
				timesLeftText.cat(", ");
			}
			timesLeftText.cat(strings->slicer);
			AppendTimeLeft(timesLeft[TimesLeft::slicer]);
			++count;
		}
		if ((count < 2 || (DisplayX >= 800 && count < 3)) && timesLeft[TimesLeft::filament] > 0)
		{
			if (count > 0) {
				timesLeftText.cat(", ");
			}
			timesLeftText.cat(strings->filament);
			AppendTimeLeft(timesLeft[TimesLeft::filament]);
			++count;
		}
		if ((count < 2 || (DisplayX >= 800 && count < 3)) && timesLeft[TimesLeft::file] > 0)
		{
			if (count > 0) {
				timesLeftText.cat(", ");
			}
			timesLeftText.cat(strings->file);
			AppendTimeLeft(timesLeft[TimesLeft::file]);
			++count;
		}

		timeLeftField->SetValue(timesLeftText.c_str());
		mgr.Show(timeLeftField, true);
	}

	void UpdateTimesLeft(size_t index, unsigned int seconds)
	{
		if (index < (int)ARRAY_SIZE(timesLeft))
		{
			timesLeft[index] = seconds;
			UpdateTimesLeftText();
		}
	}

	void UpdateDuration(uint32_t duration)
	{
		jobDuration = duration;
#if DISPLAY_X == 800
		jobStatusDuration = duration;
		if (currentUiPage == UiPage::StatusJobStatus)
		{
			RefreshJobStatusHeader();
		}
#endif
		UpdateTimesLeftText();
	}

	void UpdateWarmupDuration(uint32_t warmupDuration)
	{
		jobWarmUpDuration = warmupDuration;
		UpdateTimesLeftText();
	}

	void SetSimulatedTime(uint32_t simdTime)
	{
		simulatedTime = simdTime;
		UpdateTimesLeftText();
	}

	void SwitchToTab(ButtonBase *newTab) {
		switch (newTab->GetEvent()) {
		case evTabControl:
#if DISPLAY_X == 800
			mgr.SetRoot(controlToolsRoot);
			currentUiPage = UiPage::ControlTools;
			RefreshControlToolsPage();
			controlToolsLastTemperatureRefresh = SystemTick::GetTickCount();
#else
			mgr.SetRoot(controlRoot);
#endif
			nameField->SetValue(machineName.c_str());
			break;
		case evTabStatus:
#if DISPLAY_X == 800
			mgr.SetRoot(statusJobStatusRoot);
			currentUiPage = UiPage::StatusJobStatus;
			RefreshJobStatusTiles();
			RefreshJobStatusHeader();
			RefreshJobStatusActions();
#else
			mgr.SetRoot(printRoot);
			nameField->SetValue(
					PrintInProgress() ? printingFile.c_str() : machineName.c_str());
#endif
			break;
		case evTabSystem:
			mgr.SetRoot(messageRoot);
#if DISPLAY_X == 800
			currentUiPage = UiPage::SystemConsole;
#endif
			if (keyboardIsDisplayed)
			{
				keyboardDataHandler = SendGcode;
				mgr.SetPopup(keyboardPopup, AutoPlace, keyboardPopupY, false);
			}
			break;
		case evTabMsg:
			mgr.SetRoot(messageRoot);
			if (keyboardIsDisplayed)
			{
				keyboardDataHandler = SendGcode;
				mgr.SetPopup(keyboardPopup, AutoPlace, keyboardPopupY, false);
			}
			break;
		case evTabSetup:
			mgr.SetRoot(setupRoot);
			break;
		default:
			mgr.SetRoot(commonRoot);
			break;
		}
#if DISPLAY_X == 800
		SyncTopTabHighlight();
#endif
		mgr.Refresh(true);
	}

	// Change to the page indicated. Return true if the page has a permanently-visible button.
	static bool ChangePage(ButtonBase *newTab)
	{
		if (newTab == currentTab)
		{
			mgr.ClearAllPopups();						// if already on the correct page, just clear popups
		}
		else
		{
			if (currentTab != nullptr)
			{
				currentTab->Press(false, 0);			// remove highlighting from the old tab
				if (currentTab->GetEvent() == evTabSystem && nvData.IsSaveNeeded())
				{
					SaveSettings();						// leaving the System tab and we have changed settings, so save them
				}
			}
			newTab->Press(true, 0);						// highlight the new tab
			currentTab = newTab;
			mgr.ClearAllPopups();
			SwitchToTab(newTab);
		}
		return true;
	}

	void ShowFirmwareUpdatePopup()
	{
		mgr.SetPopup(firmwareUpdatePopup);
	}

	void ActivateScreensaver()
	{
		mgr.Show(screensaverText, isLandscape);
		mgr.SetPopup(screensaverPopup);
		lastScreensaverMoved = SystemTick::GetTickCount();
	}

	bool DeactivateScreensaver()
	{
		if (!screensaverPopup->IsPopupActive())
			return false;

		mgr.ClearPopup(true, screensaverPopup);

		return true;
	}

	void AnimateScreensaver()
	{
		if (SystemTick::GetTickCount() - lastScreensaverMoved >= ScreensaverMoveTime)
		{
			static unsigned int seed = SystemTick::GetTickCount();
			const PixelNumber width = isLandscape ? DisplayX : DisplayXP;
			const PixelNumber height = isLandscape ? DisplayY : DisplayYP;
			const PixelNumber availableWidth = (width - 2*margin - screensaverTextWidth);
			const PixelNumber availableHeight = (height - 2*margin - rowTextHeight);
			const PixelNumber x = (rand_r(&seed) % availableWidth);
			const PixelNumber y = (rand_r(&seed) % availableHeight);
			if (isLandscape)
			{
				mgr.Show(screensaverText, false);
				screensaverText->SetPosition(x + margin, y + margin);
				mgr.Show(screensaverText, true);
			}
			lastScreensaverMoved = SystemTick::GetTickCount();
		}
	}

	// Pop up the keyboard
	void ShowKeyboard()
	{
		keyboardDataHandler = SendGcode;
		mgr.SetPopup(keyboardPopup, AutoPlace, keyboardPopupY);
		keyboardIsDisplayed = true;
	}

	// This is called when the Cancel button on a popup is pressed
	void PopupCancelled()
	{
		if (mgr.GetPopup() == keyboardPopup)
		{
			keyboardIsDisplayed = false;
		}
	}

	// Return true if polling should be performed
	bool IsJobStatusPageShown()
	{
#if DISPLAY_X == 800
		return currentUiPage == UiPage::StatusJobStatus;
#else
		return false;
#endif
	}

	bool IsSetupTab()
	{
		return currentTab == tabSystem;			// don't poll while we are on the System (settings) page
	}

	void Tick()
	{
#ifdef SUPPORT_ENCODER
		encoder->Poll();
#endif
	}

#ifdef SUPPORT_ENCODER
	void HandleEncoderChange(const int change)
	{
		bool sent = false;
		if (sent) {
			lastEncoderCommandSentAt = SystemTick::GetTickCount();
		}
	}
#endif

	// This is called in the main spin loop
	void Spin()
	{
#ifdef SUPPORT_ENCODER
		if (SystemTick::GetTickCount() - lastEncoderCommandSentAt >= MinimumEncoderCommandInterval)
		{
			// Check encoder and command movement
			const int ch = encoder->GetChange();
			if (ch != 0)
			{
				HandleEncoderChange(ch);
			}
		}
#endif

		if (alertTicks != 0 && SystemTick::GetTickCount() - whenAlertReceived >= alertTicks)
		{
			ClearAlertOrResponse();
		}
#if DISPLAY_X == 800
		const uint32_t now = SystemTick::GetTickCount();
		SyncTopTabHighlight();
		if (jobAbortWhenPaused && now - jobAbortRequestedAt >= JobAbortPauseTimeoutMs)
		{
			jobAbortWhenPaused = false;			// the print never reached the paused state, so give up on the abort
		}
		if (currentUiPage == UiPage::ControlTools && now - controlToolsLastTemperatureRefresh >= ControlToolsTemperatureRefreshInterval)
		{
			controlToolsLastTemperatureRefresh = now;
			RefreshControlToolCurrentTemperatures();
			mgr.Refresh(false);
		}
		if (currentUiPage == UiPage::StatusJobStatus && now - jobStatusLastLiveRefresh >= 1000)
		{
			jobStatusLastLiveRefresh = now;
			RefreshJobStatusTiles();
			RefreshJobStatusHeader();
			mgr.Refresh(false);
		}
		if (currentUiPage == UiPage::StatusObjects && statusObjectsDirty)
		{
			RefreshStatusObjectsPage();
			const bool full = statusObjectsNeedFullRefresh;
			statusObjectsDirty = false;
			statusObjectsNeedFullRefresh = false;
			mgr.Refresh(full);
		}
#endif
	}

	// This is called when we have just started a file print
	void PrintStarted()
	{
		// ChangePage() below clears all popups. If the file start/delete popup was open, that would leave
		// currentFile set, which blocks file list updates (see IsDisplayingFileInfo).
		currentFile = nullptr;
		if (isLandscape)
		{
			ChangePage(tabStatus);
		}
	}

	// This is called when we have just received the name of the file being printed
	void PrintingFilenameChanged(const char data[])
	{
		if (!printingFile.Similar(data))
		{
			printingFile.copy(data);
			if (currentTab == tabStatus && PrintInProgress())
			{
				nameField->SetChanged();
			}
#if DISPLAY_X == 800
			RefreshJobStatusHeader();
			if (jobStatusThumbnail != nullptr)
			{
				jobStatusThumbnailCard->SetChanged();
				jobStatusThumbnail->SetChanged();
			}
#endif
		}
	}

	void LastJobFileNameAvailable(const bool available)
	{
		lastJobFileNameAvailable = available;
		if (!PrintInProgress())
		{
			mgr.Show(reprintButton, available);
		}
	}

	void SetLastFileSimulated(const bool lastFileSimulated)
	{
		TextButton* redoButton = static_cast<TextButton*>(reprintButton);
		redoButton->SetEvent(lastFileSimulated ? evResimulate : evReprint, 0);
		redoButton->SetText(lastFileSimulated ? strings->resimulate : strings->reprint);
	}

	// This is called just before the main polling loop starts. Display the default page.
	void ShowDefaultPage()
	{
		ChangePage(tabControl);
	}

	// Update the fields that are to do with the printing status
	void UpdatePrintingFields()
	{
		OM::PrinterStatus status = GetStatus();
		if (status == OM::PrinterStatus::printing || status == OM::PrinterStatus::simulating)
		{
			ShowPauseButton();
		}
		else if (status == OM::PrinterStatus::paused)
		{
			ShowResumeAndCancelButtons();
		}
		else
		{
			ShowFilesButton();
		}

		// Don't enable the time left field when we start printing, instead this will get enabled when we receive a suitable message
		if (!PrintInProgress())
		{
			mgr.Show(timeLeftField, false);
		}

		const OM::PrinterStatus stat = GetStatus();
		statusField->SetValue(((unsigned int)stat < ARRAY_SIZE(strings->statusValues) && strings->statusValues[(unsigned int)stat]) ? strings->statusValues[(unsigned int)stat] : "unknown status");
#if DISPLAY_X == 800
		RefreshJobStatusActions();
		RefreshJobStatusHeader();
#endif
	}

	// Set the percentage of print completed
	void SetPrintProgressPercent(unsigned int percent)
	{
		printProgressBar->SetPercent((uint8_t)percent);
#if DISPLAY_X == 800
		jobStatusProgress = constrain<unsigned int>(percent, 0, 100);
		if (currentUiPage == UiPage::StatusJobStatus)
		{
			RefreshJobStatusHeader();
		}
#endif
	}

	// Update the geometry or the number of axes
	void UpdateGeometry(unsigned int p_numAxes, bool p_isDelta)
	{
		if (p_numAxes != numVisibleAxes || p_isDelta != isDelta)
		{
			numVisibleAxes = p_numAxes;
			isDelta = p_isDelta;
			FileManager::RefreshMacrosList();
			numDisplayedAxes = 0;
			OM::IterateAxesWhile([](OM::Axis*& axis, size_t)
			{
				axis->slot = MaxTotalAxes;
				if (!axis->visible)
				{
					return true;
				}
				const char * letter = axis->letter;
				if (numDisplayedAxes < MaxDisplayableAxes)
				{
					axis->slot = numDisplayedAxes;
					++numDisplayedAxes;

					// Update axis letter everywhere we display it
					const uint8_t slot = axis->slot;
					controlTabAxisPos	[slot]->SetLabel(letter);
					if (moveAxisRows[slot] != nullptr)
					{
						moveAxisRows[slot]->SetValue(letter);
					}
#if DISPLAY_X == 800
					printTabAxisPos		[slot]->SetLabel(letter);
#endif
					if (movePopupAxisPos[slot] != nullptr)
					{
						movePopupAxisPos[slot]->SetLabel(letter);
					}
					homeButtons			[slot]->SetText(letter);

					// Update axis letter to be sent for homing commands
					homeButtons[slot]->SetEvent(homeButtons[slot]->GetEvent(), letter);
					homeButtons[slot]->SetColours(colours->buttonTextColour, (axis->homed) ? colours->homedButtonBackColour : colours->notHomedButtonBackColour);

					mgr.Show(homeButtons[slot], !isDelta);
					ShowAxis(slot, true, axis->letter);
				}
				// When we get here it's likely to be the initialisation phase
				// and we won't have the babystep amount set
				if (axis->letter[0] == 'Z')
				{
					if (babystepOffsetField != nullptr)		// not created in the 800x480 build
					{
						babystepOffsetField->SetValue(axis->babystep);
					}
				}
				return true;
			});
			// Hide axes possibly shown before
			for (size_t i = numDisplayedAxes; i < MaxDisplayableAxes; ++i)
			{
				mgr.Show(homeButtons[i], false);
				ShowAxis(i, false);
			}
		}
	}

	void UpdateAllHomed()
	{
		bool allHomed = true;
		OM::IterateAxesWhile([&allHomed](OM::Axis*& axis, size_t) {
			if (axis->visible && !axis->homed)
			{
				allHomed = false;
				return false;
			}
			return true;
		});
		if (allHomed != allAxesHomed)
		{
			allAxesHomed = allHomed;
			homeAllButton->SetColours(colours->buttonTextColour, (allAxesHomed) ? colours->homedButtonBackColour : colours->notHomedButtonBackColour);
		}
		#if DISPLAY_X == 800
		RefreshControlMoveHoming();
		#endif
	}

	// Update the homed status of the specified axis. If the axis is -1 then it represents the "all homed" status.
	void UpdateHomedStatus(size_t axisIndex, bool isHomed)
	{
		OM::Axis *axis = OM::GetOrCreateAxis(axisIndex);
		if (axis == nullptr)
		{
			return;
		}
		axis->homed = isHomed;
		const size_t slot = axis->slot;
		if (slot < MaxDisplayableAxes)
		{
			homeButtons[slot]->SetColours(colours->buttonTextColour, (isHomed) ? colours->homedButtonBackColour : colours->notHomedButtonBackColour);
		}

		UpdateAllHomed();
	}

	// Update the Z probe text
	void UpdateZProbe(const char data[])
	{
		zprobeBuf.copy(data);
		zProbe->SetChanged();
	}

	// Update the machine name
	void UpdateMachineName(const char data[])
	{
		machineName.copy(data);
		nameField->SetChanged();
	}

	// Update the IP address fiels on Setup tab
	void UpdateIP(const char data[])
	{
		ipAddress.copy(data);
		if (ipAddressField != nullptr)
		{
			ipAddressField->SetChanged();
		}
#if DISPLAY_X == 800
		if (settingsIpValueButton != nullptr)
		{
			settingsIpText.copy(ipAddress.c_str());
			settingsIpValueButton->SetText(settingsIpText.c_str());
		}
#endif
	}

	// Update the fan RPM
	void UpdateFanName(size_t fanIndex, const char *name)
	{
		if (fanIndex < TuneMaxFans)
		{
			tuneFanNames[fanIndex].copy((name != nullptr) ? name : "");
			if (currentUiPage == UiPage::StatusJobStatus)
			{
				RefreshJobStatusTilesByType(JobStatusTileType::FanAux);
				RefreshJobStatusTilesByType(JobStatusTileType::FanCha);
			}
			else if (currentUiPage == UiPage::StatusTune)
			{
				RefreshTuneGeneralFans();
			}
		}
	}

	// Remember which fans are thermostatically controlled. It decides which of a tool's fans is its part cooling fan.
	void SetFanThermostatic(size_t fanIndex, bool thermostatic)
	{
#if DISPLAY_X == 800
		if (fanIndex < TuneMaxFans && tuneFanThermostatic[fanIndex] != thermostatic)
		{
			tuneFanThermostatic[fanIndex] = thermostatic;
			if (currentUiPage == UiPage::StatusTune)
			{
				RefreshTuneToolRows();
			}
			else if (currentUiPage == UiPage::StatusJobStatus)
			{
				RefreshJobStatusTilesByType(JobStatusTileType::FanPart);
			}
		}
#else
		UNUSED(fanIndex); UNUSED(thermostatic);
#endif
	}

	// Set the number of fans currently exposed by RRF. This clears stale
	// cached fan names/values when a printer configuration removes fans.
	void SetFanCount(size_t count)
	{
#if DISPLAY_X == 800
		const size_t validCount = (count < TuneMaxFans) ? count : TuneMaxFans;
		for (size_t fan = validCount; fan < TuneMaxFans; ++fan)
		{
			tuneFanPercent[fan] = 0;
			tuneFanValid[fan] = false;
			tuneFanThermostatic[fan] = false;
			tuneFanNames[fan].Clear();
		}

		if (currentUiPage == UiPage::StatusJobStatus)
		{
			RefreshJobStatusTilesByType(JobStatusTileType::FanPart);
			RefreshJobStatusTilesByType(JobStatusTileType::FanAux);
			RefreshJobStatusTilesByType(JobStatusTileType::FanCha);
		}
#endif
	}

	void UpdateFanPercent(size_t fanIndex, int rpm)
	{
#if DISPLAY_X == 800
		if (fanIndex < TuneMaxFans)
		{
			tuneFanPercent[fanIndex] = constrain<int>(rpm, 0, 100);
			tuneFanValid[fanIndex] = true;
			if (currentUiPage == UiPage::StatusJobStatus)
			{
				RefreshJobStatusTilesByType(JobStatusTileType::FanPart);
				RefreshJobStatusTilesByType(JobStatusTileType::FanAux);
				RefreshJobStatusTilesByType(JobStatusTileType::FanCha);
			}
		}
#endif
		if (currentTool == NoTool)
		{
			if (fanIndex == 0)
			{
				UpdateField(fanSpeed, rpm);
			}
		}
		else
		{
			// There might be multiple tools using the same fan and one of them might
			// be the active one but not necessarily the first one so we need to iterate
			OM::IterateToolsWhile([&fanIndex, &rpm](OM::Tool*& tool, size_t) {
				if (tool->index == currentTool && tool->fans.IsBitSet(fanIndex) && GetToolPartFan(tool) == static_cast<int>(fanIndex))
				{
					UpdateField(fanSpeed, rpm);
				}
				return true;
			});
		}
	}

	void UpdateToolTemp(size_t toolIndex, size_t toolHeaterIndex, int32_t temp, bool active)
	{
		OM::Tool *tool = OM::GetOrCreateTool(toolIndex);

		// If we do not handle this tool back off
		if (tool == nullptr)
		{
			return;
		}

#if DISPLAY_X == 800
		// RRF may report the same target repeatedly. A full TOOLS-page rebuild for
		// an unchanged target needlessly redraws every tile/icon and causes visible
		// flashing, so only rebuild when the value actually changes.
		const bool targetChanged = (toolHeaterIndex == 0) &&
			(tool->heaters[0] == nullptr ||
			 (active ? tool->heaters[0]->activeTemp : tool->heaters[0]->standbyTemp) != temp);
#endif

		tool->UpdateTemp(toolHeaterIndex, temp, active);
		if (toolHeaterIndex == 0 || nvData.GetHeaterCombineType() == HeaterCombineType::notCombined)
		{
			if (tool->slot + toolHeaterIndex < MaxSlots)
			{
				UpdateField((active ? activeTemps : standbyTemps)[tool->slot + toolHeaterIndex], temp);
			}
		}
#if DISPLAY_X == 800
		if (targetChanged && currentUiPage == UiPage::ControlTools)
		{
			RefreshControlToolsPage();
		}
#endif
	}

	void UpdateTemperature(size_t heaterIndex, int ival, IntegerButton** fields)
	{
		OM::Slots heaterSlots;
		OM::GetHeaterSlots(heaterIndex, heaterSlots, false);	// Ignore tools
		if (!heaterSlots.IsEmpty())
		{
			const size_t count = heaterSlots.Size();
			for (size_t i = 0; i < count; ++i)
			{
				UpdateField(fields[heaterSlots[i]], ival);
			}

			heaterSlots.Clear();
		}
	}

	// Update an active temperature
	void UpdateActiveTemperature(size_t index, int ival)
	{
#if DISPLAY_X == 800
		if (index < ControlToolMaxHeaters)
		{
			const bool targetChanged = (controlToolActiveTarget[index] != ival);
			controlToolActiveTarget[index] = ival;
			if (targetChanged && currentUiPage == UiPage::ControlTools) RefreshControlToolsPage();
		}
#endif
		UpdateTemperature(index, ival, activeTemps);
	}

	// Update a standby temperature
	void UpdateStandbyTemperature(size_t index, int ival)
	{
#if DISPLAY_X == 800
		if (index < ControlToolMaxHeaters)
		{
			const bool targetChanged = (controlToolStandbyTarget[index] != ival);
			controlToolStandbyTarget[index] = ival;
			if (targetChanged && currentUiPage == UiPage::ControlTools) RefreshControlToolsPage();
		}
#endif
		UpdateTemperature(index, ival, standbyTemps);
	}

#if DISPLAY_X == 800
	void UpdateColdExtrudeTemperature(float value)
	{
		controlColdExtrudeTemperature = value;
		controlColdExtrudeTemperatureValid = true;
	}

	void UpdateColdRetractTemperature(float value)
	{
		controlColdRetractTemperature = value;
		controlColdRetractTemperatureValid = true;
	}
#else
	void UpdateColdExtrudeTemperature(float value) { UNUSED(value); }
	void UpdateColdRetractTemperature(float value) { UNUSED(value); }
#endif

	// Update an extrusion factor
	void UpdateExtrusionFactor(size_t index, int ival)
	{
#if DISPLAY_X == 800
		if (index < TuneMaxExtruders)
		{
			tuneExtruderFactor[index] = ival;
			if (currentUiPage == UiPage::StatusJobStatus)
			{
				RefreshJobStatusTilesByType(JobStatusTileType::FlowFactor);
			}
		}
#endif
		OM::IterateToolsWhile([&index, &ival](OM::Tool*& tool, size_t) {
			if (tool->extruders.IsBitSet(index) && tool->slot < MaxSlots)
			{
				UpdateField(extrusionFactors[tool->slot], ival);
			}
			return tool->slot < MaxSlots;
		});
	}

	// Update the print speed factor
	void UpdateSpeedPercent(int ival)
	{
		UpdateField(spd, ival);
#if DISPLAY_X == 800
		// Cache only. TUNE is intentionally event-driven and repaints on tab
		// entry/page changes or immediately after a user-confirmed change.
		tuneSpeedPercent = ival;
#endif
	}

	void UpdateJobLayer(unsigned int layer)
	{
#if DISPLAY_X == 800
		jobStatusLayer = layer;
		if (currentUiPage == UiPage::StatusJobStatus) RefreshJobStatusHeader();
#else
		UNUSED(layer);
#endif
	}

	void UpdateJobNumLayers(unsigned int layers)
	{
#if DISPLAY_X == 800
		jobStatusNumLayers = layers;
		if (currentUiPage == UiPage::StatusJobStatus) RefreshJobStatusHeader();
#else
		UNUSED(layers);
#endif
	}

	void UpdateCurrentMoveRequestedSpeed(float value)
	{
#if DISPLAY_X == 800
		jobStatusRequestedSpeed = value;
#else
		UNUSED(value);
#endif
	}

	void UpdateCurrentMoveTopSpeed(float value)
	{
#if DISPLAY_X == 800
		jobStatusTopSpeed = value;
#else
		UNUSED(value);
#endif
	}

	void UpdateCurrentMoveExtrusionRate(float value)
	{
#if DISPLAY_X == 800
		jobStatusExtrusionRate = value;
		const uint32_t now = SystemTick::GetTickCount();
		const uint32_t elapsed = now - jobStatusExtrusionRateTime;
		if (!jobStatusExtrusionRateHaveAvg || elapsed > JobStatusFlowStaleMs)
		{
			jobStatusExtrusionRateAvg = value;					// start (or restart after a gap) from the first sample
			jobStatusExtrusionRateHaveAvg = true;
		}
		else
		{
			// Exponential moving average whose weight depends on the time since the previous sample, so it behaves the
			// same whether samples arrive every 0.5 s or every 1.5 s.
			const float alpha = static_cast<float>(elapsed) / (static_cast<float>(elapsed) + static_cast<float>(JobStatusFlowAverageMs));
			jobStatusExtrusionRateAvg += (value - jobStatusExtrusionRateAvg) * alpha;
		}
		jobStatusExtrusionRateTime = now;
#else
		UNUSED(value);
#endif
	}

	void UpdateFilamentDiameter(size_t extruder, float value)
	{
#if DISPLAY_X == 800
		if (extruder < JobStatusMaxExtruders)
		{
			jobStatusFilamentDiameter[extruder] = value;
			jobStatusFilamentDiameterValid[extruder] = value > 0.0f;
		}
#else
		UNUSED(extruder); UNUSED(value);
#endif
	}

	void UpdatePressureAdvance(size_t index, float value)
	{
#if DISPLAY_X == 800
		if (index < TuneMaxExtruders)
		{
			tunePressureAdvance[index] = value;
			tunePressureAdvanceValid[index] = true;
		}
#else
		UNUSED(index);
		UNUSED(value);
#endif
	}

	void UpdateStatusCurrentObject(int objectIndex)
	{
#if DISPLAY_X == 800
		currentStatusObject = objectIndex;
#else
		UNUSED(objectIndex);
#endif
	}

	void UpdateStatusObjectName(size_t objectIndex, const char *name)
	{
#if DISPLAY_X == 800
		if (objectIndex >= StatusMaxObjects) return;
		StatusObjectInfo& obj = statusObjects[objectIndex];
		bool changed = !obj.present;
		obj.present = true;
		if (name == nullptr || strcasecmp(name, "null") == 0)
		{
			if (!obj.name.IsEmpty())
			{
				obj.name.Clear();
				changed = true;
			}
		}
		else if (!obj.name.Equals(name))
		{
			obj.name.copy(name);
			changed = true;
		}
		if (changed) statusObjectsDirty = true;
#else
		UNUSED(objectIndex); UNUSED(name);
#endif
	}

	void UpdateStatusObjectCancelled(size_t objectIndex, bool cancelled)
	{
#if DISPLAY_X == 800
		if (objectIndex >= StatusMaxObjects) return;
		StatusObjectInfo& obj = statusObjects[objectIndex];
		if (!obj.present || obj.cancelled != cancelled) statusObjectsDirty = true;
		obj.present = true;
		obj.cancelled = cancelled;
#else
		UNUSED(objectIndex); UNUSED(cancelled);
#endif
	}

	// The coordinates of an object arrive as a two element array [min, max], repeated in every job update. So that an
	// unchanged object does not force a repaint (which made the map blink), the new values are collected first and only
	// committed, and the page marked dirty, when they differ from what is already stored.
	static size_t coordObject = 0;
	static bool coordIsX = true;
	static bool coordActive = false;
	static uint8_t coordCount = 0;
	static float coordMin = 0.0f, coordMax = 0.0f;

	void BeginStatusObjectCoordinate(size_t objectIndex, bool xAxis)
	{
#if DISPLAY_X == 800
		if (objectIndex >= StatusMaxObjects) return;
		if (!statusObjects[objectIndex].present)
		{
			statusObjects[objectIndex].present = true;
			statusObjectsDirty = true;
		}
		coordObject = objectIndex;
		coordIsX = xAxis;
		coordActive = true;
		coordCount = 0;
#else
		UNUSED(objectIndex); UNUSED(xAxis);
#endif
	}

	void ClearStatusObjectCoordinate(size_t objectIndex, bool xAxis)
	{
#if DISPLAY_X == 800
		if (objectIndex >= StatusMaxObjects) return;
		StatusObjectInfo& obj = statusObjects[objectIndex];
		if (!obj.present || (xAxis ? obj.xValid : obj.yValid)) statusObjectsDirty = true;
		obj.present = true;
		if (xAxis) obj.xValid = false;
		else obj.yValid = false;
		if (coordActive && coordObject == objectIndex && coordIsX == xAxis)
		{
			coordActive = false;
		}
#else
		UNUSED(objectIndex); UNUSED(xAxis);
#endif
	}

	void UpdateStatusObjectCoordinate(size_t objectIndex, bool xAxis, float value)
	{
#if DISPLAY_X == 800
		if (objectIndex >= StatusMaxObjects) return;
		StatusObjectInfo& obj = statusObjects[objectIndex];
		if (!obj.present)
		{
			obj.present = true;
			statusObjectsDirty = true;
		}
		if (!coordActive || coordObject != objectIndex || coordIsX != xAxis)
		{
			// Not part of a sequence started by BeginStatusObjectCoordinate(): treat it as the first value
			coordObject = objectIndex;
			coordIsX = xAxis;
			coordActive = true;
			coordCount = 0;
		}
		if (coordCount == 0)
		{
			coordMin = coordMax = value;
		}
		else
		{
			if (value < coordMin) coordMin = value;
			if (value > coordMax) coordMax = value;
		}
		++coordCount;
		if (coordCount >= 2)
		{
			// Both ends of the range have arrived: commit only if it differs from what is stored
			bool& valid = xAxis ? obj.xValid : obj.yValid;
			float& minValue = xAxis ? obj.xMin : obj.yMin;
			float& maxValue = xAxis ? obj.xMax : obj.yMax;
			if (!valid || minValue != coordMin || maxValue != coordMax)
			{
				minValue = coordMin;
				maxValue = coordMax;
				valid = true;
				statusObjectsDirty = true;
			}
			coordActive = false;
		}
#else
		UNUSED(objectIndex); UNUSED(xAxis); UNUSED(value);
#endif
	}

	void UpdateStatusObjectCount(size_t count)
	{
#if DISPLAY_X == 800
		const unsigned int newCount = static_cast<unsigned int>((count > StatusMaxObjects) ? StatusMaxObjects : count);
		bool somethingChanged = (statusObjectCount != newCount);
		for (unsigned int i = 0; i < newCount; ++i)
		{
			if (!statusObjects[i].present)
			{
				statusObjects[i].present = true;
				somethingChanged = true;
			}
		}
		for (unsigned int i = newCount; i < StatusMaxObjects; ++i)
		{
			StatusObjectInfo& obj = statusObjects[i];
			if (obj.present || obj.cancelled || obj.xValid || obj.yValid || !obj.name.IsEmpty())
			{
				somethingChanged = true;
			}
			obj.present = false;
			obj.cancelled = false;
			obj.xValid = false;
			obj.yValid = false;
			obj.name.Clear();
		}
		if (statusObjectCount != newCount) statusObjectsNeedFullRefresh = true;
		statusObjectCount = newCount;
		if (selectedStatusObject >= static_cast<int>(newCount)) selectedStatusObject = -1;
		if (newCount == 0 && currentStatusObject >= 0) currentStatusObject = -1;
		if (somethingChanged) statusObjectsDirty = true;
#else
		UNUSED(count);
#endif
	}

	// Process a new message box alert, clearing any existing one
	void ProcessAlert(const Alert& alert)
	{
#if DISPLAY_X == 800
		PrepareStandardPopupForIncomingM291();
#endif
		if (isLandscape)
		{
#if DISPLAY_X == 800
			if (alert.mode == Alert::Mode::Info || alert.mode == Alert::Mode::InfoClose)
			{
				ShowModernInfoPopup(alert.title.c_str(), alert.text.c_str(), false);
			}
			else if ((alert.mode == Alert::Mode::InfoConfirm || alert.mode == Alert::Mode::ConfirmCancel) && alert.controls != 0)
			{
				ShowModernM291ConfirmControlsPopup(alert);
			}
			else if (alert.mode == Alert::Mode::InfoConfirm || alert.mode == Alert::Mode::ConfirmCancel)
			{
				ShowModernM291ConfirmPopup(alert);
			}
			else if (alert.mode == Alert::Mode::Choices)
			{
				ShowModernM291ChoicesPopup(alert);
			}
			else if (alert.mode == Alert::Mode::NumberInt)
			{
				ShowModernM291NumberIntPopup(alert);
			}
			else if (alert.mode == Alert::Mode::NumberFloat)
			{
				ShowModernM291NumberFloatPopup(alert);
			}
			else if (alert.mode == Alert::Mode::Text)
			{
				ShowModernM291TextPopup(alert);
			}
			else
			{
				ShowModernInfoPopup(alert.title.c_str(), alert.text.c_str(), false);
			}
#else
			alertPopup->Set(alert);
			mgr.SetPopup(alertPopup, AutoPlace, AutoPlace);
#endif
		}
		alertMode = alert.mode;
		displayingResponse = false;
		whenAlertReceived = SystemTick::GetTickCount();
		alertTicks = (alertMode < 2) ? (uint32_t)(alert.timeout * 1000.0) : 0;
	}

	// Process a command to clear a message box alert
	void ClearAlert()
	{
		if (alertMode >= 0)
		{
			alertTicks = 0;
#if DISPLAY_X == 800
			if (ClearSharedM291AlertPopup())
			{
			}
			else if (displayingModernInfoPopup)
			{
				ClearDisplayedModernInfoPopup();
			}
#else
			mgr.ClearPopup(true, alertPopup);
#endif
			CurrentAlertModeClear();
			alertMode = -1;
		}
	}

	// Clear a message box alert or response. Called when the user presses the close button or the alert or response times out.
	void ClearAlertOrResponse()
	{
		if (alertMode >= 0 || displayingResponse)
		{
			alertTicks = 0;
#if DISPLAY_X == 800
			if (ClearSharedM291AlertPopup())
			{
			}
			else if (displayingModernInfoPopup)
			{
				ClearDisplayedModernInfoPopup();
			}
#else
			mgr.ClearPopup(true, alertPopup);
#endif
			CurrentAlertModeClear();
			alertMode = -1;
			displayingResponse = false;
		}
	}

	bool CanDimDisplay()
	{
		return alertMode < 2;
	}

	void ProcessSimpleAlert(const char* _ecv_array text)
	{
		if (alertMode < 2)												// if the current alert doesn't require acknowledgement
		{
			if (isLandscape)
			{
#if DISPLAY_X == 800
				ShowModernInfoPopup("", text, false);
#else
				alertPopup->Set(strings->message, text, 1, 0);
				mgr.SetPopup(alertPopup, AutoPlace, AutoPlace);
#endif
			}
			alertMode = 1;												// a simple alert is like a mode 1 alert without a title
			displayingResponse = false;
			whenAlertReceived = SystemTick::GetTickCount();
			alertTicks = 0;												// no timeout
		}
	}

	// Process a new response. This is treated like a simple alert except that it times out and isn't cleared by a "clear alert" command from the host.
	void NewResponseReceived(const char* _ecv_array text)
	{
		const bool isErrorMessage = StringStartsWith(text, "Error");
		if (   alertMode < 2											// if the current alert doesn't require acknowledgement
			&& currentTab != tabSystem										// avoid overlaying responses while viewing SYSTEM
			&& (isErrorMessage || infoTimeout != 0)
		   )
		{
			if (isLandscape)
			{
#if DISPLAY_X == 800
				ShowModernInfoPopup(isErrorMessage ? "ALERT !" : "", text, isErrorMessage);
#else
				alertPopup->Set(strings->response, text, 1, 0);
				mgr.SetPopup(alertPopup, AutoPlace, AutoPlace);
#endif
			}
			alertMode = -1;												// make sure that a call to ClearAlert doesn't clear us
			displayingResponse = true;
			whenAlertReceived = SystemTick::GetTickCount();
			alertTicks = isErrorMessage ? 0 : infoTimeout * SystemTick::TicksPerSecond;				// time out if it isn't an error message
		}
	}

	// This is called when the user selects a new file from a list of SD card files
	void FileSelected(const char * _ecv_array null fileName)
	{
		if (fpNameField == nullptr)
		{
			return;		// legacy file info popup is not created in the 800x480 build
		}
		fpNameField->SetValue(fileName);
		// Clear out the old field values, they relate to the previous file we looked at until we process the response
		fpSizeField->SetValue(0);						// would be better to make it blank
		fpHeightField->SetValue(0.0);					// would be better to make it blank
		fpLayerHeightField->SetValue(0.0);				// would be better to make it blank
		fpFilamentField->SetValue(0);					// would be better to make it blank
		generatedByText.Clear();
		fpGeneratedByField->SetChanged();
		lastModifiedText.Clear();
		fpLastModifiedField->SetChanged();
		printTimeText.Clear();
		fpPrintTimeField->SetChanged();
	}

	// This is called when the "generated by" file information has been received
	void UpdateFileGeneratedByText(const char data[])
	{
		if (fpGeneratedByField == nullptr)
		{
			return;		// legacy file info popup is not created in the 800x480 build
		}
		generatedByText.copy(data);
		fpGeneratedByField->SetChanged();
	}

	// This is called when the "last modified" file information has been received
	void UpdateFileLastModifiedText(const char data[])
	{
		if (fpLastModifiedField == nullptr)
		{
			return;
		}
		lastModifiedText.copy(data);
		lastModifiedText.Replace('T', ' ');
		lastModifiedText.Replace('+', '\0');		// ignore time zone if present
		lastModifiedText.Replace('.', '\0');		// ignore decimal seconds if present (DCS 2.0.0 sends them)
		fpLastModifiedField->SetChanged();
	}

	// This is called when the "last modified" file information has been received
	void UpdatePrintTimeText(uint32_t seconds, bool isSimulated)
	{
		if (fpPrintTimeField == nullptr)
		{
			return;
		}
		bool update = false;
		if (isSimulated)
		{
			printTimeText.Clear();					// prefer simulated to estimated print time
			fpPrintTimeField->SetLabel(strings->simulatedPrintTime);
			update = true;
		}
		else if (printTimeText.IsEmpty())
		{
			fpPrintTimeField->SetLabel(strings->estimatedPrintTime);
			update = true;
		}
		if (update)
		{
			unsigned int minutes = (seconds + 50)/60;
			printTimeText.printf("%dh %02dm", minutes / 60, minutes % 60);
			fpPrintTimeField->SetChanged();
		}
	}

	// This is called when the object height information for the file has been received
	void UpdateFileObjectHeight(float f)
	{
		if (fpHeightField != nullptr)
		{
			fpHeightField->SetValue(f);
		}
	}

	// This is called when the layer height information for the file has been received
	void UpdateFileLayerHeight(float f)
	{
		if (fpLayerHeightField != nullptr)
		{
			fpLayerHeightField->SetValue(f);
		}
	}

	// This is called when the size of the file has been received
	void UpdateFileSize(int size)
	{
		if (fpSizeField != nullptr)
		{
			fpSizeField->SetValue(size);
		}
	}

	// This is called when the filament needed by the file has been received
	void UpdateFileFilament(int len)
	{
		if (fpFilamentField != nullptr)
		{
			fpFilamentField->SetValue(len);
		}
	}

	unsigned int GetThumbnailTargetWidth()
	{
#if DISPLAY_X == 800
		if (currentUiPage == UiPage::StatusJobStatus && jobStatusThumbnail != nullptr)
		{
			return jobStatusThumbnail->GetWidth();
		}
#endif
		return (mgr.IsPopupActive(fileDetailPopup) && fpThumbnail != nullptr) ? fpThumbnail->GetWidth() : 0;
	}

	unsigned int GetThumbnailTargetHeight()
	{
#if DISPLAY_X == 800
		if (currentUiPage == UiPage::StatusJobStatus && jobStatusThumbnail != nullptr)
		{
			return jobStatusThumbnail->GetHeight();
		}
#endif
		return (mgr.IsPopupActive(fileDetailPopup) && fpThumbnail != nullptr) ? fpThumbnail->GetHeight() : 0;
	}

	bool UpdateFileThumbnailChunk(const struct Thumbnail &thumbnail, uint32_t pixels_offset, const qoi_rgba_t *pixels, size_t pixels_count)
	{
		DrawDirect *target = nullptr;
#if DISPLAY_X == 800
		if (currentUiPage == UiPage::StatusJobStatus && jobStatusThumbnail != nullptr)
		{
			target = jobStatusThumbnail;
		}
#endif
		if (target == nullptr && mgr.IsPopupActive(fileDetailPopup))
		{
			target = fpThumbnail;
		}
		if (target == nullptr || pixels == nullptr)
		{
			return false;
		}

		// QOI decoding is chunked (currently up to 64 pixels). Convert each chunk immediately
		// to the LCD's native RGB565 format; no full thumbnail framebuffer is allocated.
		uint16_t rgb565[64];
		if (pixels_count > ARRAY_SIZE(rgb565))
		{
			return false;
		}
		for (size_t i = 0; i < pixels_count; ++i)
		{
			const uint8_t r = pixels[i].rgba.r;
			const uint8_t g = pixels[i].rgba.g;
			const uint8_t b = pixels[i].rgba.b;
			rgb565[i] = static_cast<uint16_t>(((r & 0xF8u) << 8) | ((g & 0xFCu) << 3) | (b >> 3));
		}
		target->DrawRect565(thumbnail.width, thumbnail.height, pixels_offset, rgb565, pixels_count);
		return true;
	}

	// Return true if we are displaying file information
	bool IsDisplayingFileInfo()
	{
		return currentFile != nullptr;
	}

	static void DoEmergencyStop()
	{
		// We send M112 for the benefit of old firmware, and F0 0F (an invalid UTF8 sequence) for new firmware
		SerialIo::Sendf("M112 ;" "\xF0" "\x0F" "\n");
		TouchBeep();											// needed when we are called from ProcessTouchOutsidePopup
		Delay(1000);
		SerialIo::Sendf("M999\n");
		Delay(1000);
	}

	// Make this into a template if we need something else than IntegerButton** as list
	size_t GetButtonSlot(IntegerButton** buttonList, ButtonBase* button)
	{
		size_t slot = MaxSlots;
		for (size_t i = 0; i < MaxSlots; ++i)
		{
			if (buttonList[i] == button)
			{
				slot = i;
				break;
			}
		}
		return slot;
	}

	void ProcessRelease(ButtonPress bp)
	{
		if (!bp.IsValid())
		{
			return;
		}

		ButtonBase *f = bp.GetButton();
		Event ev = (Event)(f->GetEvent());

		switch(ev)
		{
		case evTabControl:
		case evTabStatus:
		case evTabSystem:
		case evTabMsg:
		case evTabSetup:
#if DISPLAY_X == 800
		case evControlTools:				// top tabs: their highlight is managed by SyncTopTabHighlight()
		case evControlMovement:
		case evControlExtrusion:
		case evControlMacros:
		case evStatusJobStatus:
		case evStatusTune:
		case evStatusJob:
		case evStatusObjects:
		case evSystemConsole:
		case evSystemSettings:
#endif

		case evExtrudeAmount:
		case evExtrudeRate:

		case evAdjustBaudRate:
		case evAdjustVolume:
		case evAdjustInfoTimeout:
		case evAdjustScreensaverTimeout:
		case evAdjustBabystepAmount:
		case evAdjustFeedrate:
		case evAdjustColours:
			break;
		case evOkAlert:
		case evCloseAlert:
		case evChoiceAlert:
			mgr.Press(bp, false);
			ClearAlertOrResponse();
			break;
		default:
			mgr.Press(bp, false);
			break;
		}
	}

#if DISPLAY_X == 800
	static bool KeepModernButtonHighlighted(ButtonBase *button)
	{
		if (button == tabControl || button == tabStatus || button == tabSystem)
		{
			return true;
		}
		// Modern sub-tabs occupy the fixed y=0 top-tab row. Their pressed state
		// is the selected-tab indication and must persist after finger-up.
		return button != nullptr && button->GetMinY() == 0 && button->GetMinX() >= topTabRowLeft;
	}
#endif

	// Process a touch event
	void ProcessTouch(ButtonPress bp)
	{
		if (bp.IsValid())
		{
			ButtonBase *f = bp.GetButton();
			currentButton = bp;
			mgr.Press(bp, true);
			Event ev = (Event)(f->GetEvent());


			if (bp.GetEvent() != evAdjustVolume)
			{
				TouchBeep();		// give audible feedback of the touch, unless adjusting the volume
			}

			switch(ev)
			{
			case evEmergencyStop:
				DoEmergencyStop();
				break;

			case evTabControl:
			case evTabStatus:
			case evTabSystem:
			case evTabMsg:
			case evTabSetup:
				if (ChangePage(f))
				{
					currentButton.Clear();						// keep the button highlighted after it is released
				}
				break;

			case evControlTools:
#if DISPLAY_X == 800
				mgr.SetRoot(controlToolsRoot);
				currentUiPage = UiPage::ControlTools;
				RefreshControlToolsPage();
				controlToolsLastTemperatureRefresh = SystemTick::GetTickCount();
#else
				mgr.SetRoot(controlRoot);
#endif
				mgr.Refresh(true);
				currentButton.Clear();
				break;

			case evControlMovement:
#if DISPLAY_X == 800
				if (IsModernPageLockedByPrint(false))
				{
					mgr.Press(bp, false);
					currentButton.Clear();
					ShowModernAlert("MOVE is unavailable while a print job is active.");
					break;
				}
				mgr.SetRoot(controlMovementRoot);
				RefreshControlMoveSteps();
				RefreshControlMoveHoming();
#else
				mgr.SetRoot(controlRoot);
#endif
				currentUiPage = UiPage::ControlMovement;
				mgr.Refresh(true);
				currentButton.Clear();
				break;

			case evControlExtrusion:
#if DISPLAY_X == 800
				if (IsModernPageLockedByPrint(true))
				{
					mgr.Press(bp, false);
					currentButton.Clear();
					ShowModernAlert("EXTRUDE is unavailable while the print is running.");
					break;
				}
				mgr.SetRoot(controlExtrusionRoot);
				SelectControlExtrudePageForActiveTool();
				RefreshControlExtrudeTools();
				RefreshControlExtrudeSelections();
#else
				mgr.SetRoot(controlRoot);
#endif
				currentUiPage = UiPage::ControlExtrusion;
				mgr.Refresh(true);
				currentButton.Clear();
				break;

			case evControlMacros:
#if DISPLAY_X == 800
				if (IsModernPageLockedByPrint(false))
				{
					mgr.Press(bp, false);
					currentButton.Clear();
					ShowModernAlert("MACROS are unavailable while a print job is active.");
					break;
				}
				mgr.SetRoot(controlMacrosRoot);
				FileManager::DisplayControlMacrosPage();
#else
				mgr.SetRoot(controlRoot);
#endif
				currentUiPage = UiPage::ControlMacros;
				mgr.Refresh(true);
				currentButton.Clear();
				break;

#if DISPLAY_X == 800
			case evControlMacroFile:
			{
				const char * const macroName = bp.GetSParam();
				if (macroName == nullptr)
				{
					ErrorBeep();
					break;
				}
				if (macroName[0] == '*')
				{
					FileManager::RequestControlMacrosSubdir(macroName + 1);
				}
				else
				{
					controlMacroPendingFile.copy(macroName);
					standardPopupContext = StandardPopupContext::ControlMacroRun;
					ConfigureStandardPopupTitle("RUN MACRO", false);
					ResetStandardPopupContent();
					ConfigureStandardPopupInformation("Do you want to run this macro?", SkipDigitsAndUnderscore(controlMacroPendingFile.c_str()));
					standardPopupCancelButton->Show(true);
					standardPopupConfirmButton->Show(true);
					mgr.SetPopup(standardPopup, AutoPlace, AutoPlace);
				}
				currentButton.Clear();
				break;
			}

			case evControlMacroPageUp:
				mgr.Press(bp, false);
				currentButton.Clear();
				if (controlMacroCanScrollEarlier)
				{
					FileManager::ScrollControlMacrosPage(-static_cast<int>(ControlMacroRows));
				}
				else if (controlMacroInSubdir)
				{
					FileManager::RequestControlMacrosParentDir();
				}
				currentButton.Clear();
				break;

			case evControlMacroPageDown:
				mgr.Press(bp, false);
				currentButton.Clear();
				if (controlMacroCanScrollLater)
				{
					FileManager::ScrollControlMacrosPage(static_cast<int>(ControlMacroRows));
				}
				currentButton.Clear();
				break;

			case evControlToolsPageUp:
				if (controlToolPage > 0) --controlToolPage;
				RefreshControlToolsPage();
				controlToolsLastTemperatureRefresh = SystemTick::GetTickCount();
				mgr.Refresh(false);
				currentButton.Clear();
				break;

			case evControlToolsPageDown:
				++controlToolPage;
				RefreshControlToolsPage();
				controlToolsLastTemperatureRefresh = SystemTick::GetTickCount();
				mgr.Refresh(false);
				currentButton.Clear();
				break;

			case evControlToolsActiveTemp:
				OpenControlTempNumpad(static_cast<unsigned int>(bp.GetIParam()), true);
				currentButton.Clear();
				break;

			case evControlToolsStandbyTemp:
				OpenControlTempNumpad(static_cast<unsigned int>(bp.GetIParam()), false);
				currentButton.Clear();
				break;

			case evControlToolsHeaderTap:
				HandleControlToolHeaderTap(static_cast<unsigned int>(bp.GetIParam()));
				currentButton.Clear();
				break;

			case evControlToolsPower:
				HandleControlToolPower(static_cast<unsigned int>(bp.GetIParam()));
				currentButton.Clear();
				break;

			case evNumericKey:
				if (controlTempNumpadPopup != nullptr && mgr.GetPopup() == controlTempNumpadPopup)
				{
					const unsigned int key = static_cast<unsigned int>(bp.GetIParam());
					if (numericPopupContext == NumericPopupContext::Temperature)
					{
						if (key <= 9)
						{
							if (controlTempNumpadFresh)
							{
								controlTempNumpadValue = key;
								controlTempNumpadFresh = false;
							}
							else if (controlTempNumpadValue <= 99)
							{
								controlTempNumpadValue = controlTempNumpadValue * 10 + key;
							}
							RefreshControlTempNumpadValue();
						}
					}
					else
					{
						if (key <= 9)
						{
							if (numericPopupM291Fresh)
							{
								numericPopupM291Text.Clear();
								numericPopupM291Fresh = false;
							}
							if (numericPopupM291Text.strlen() + 1 < numericPopupM291Text.Capacity())
							{
								numericPopupM291Text.cat((char)('0' + key));
							}
						}
						else if (key == 10 && numericPopupContext == NumericPopupContext::M291Float)
						{
							if (strchr(numericPopupM291Text.c_str(), '.') == nullptr)
							{
								if (numericPopupM291Fresh)
								{
									numericPopupM291Text.copy("0");
									numericPopupM291Fresh = false;
								}
								if (numericPopupM291Text.strlen() + 1 < numericPopupM291Text.Capacity())
								{
									numericPopupM291Text.cat('.');
								}
							}
						}
						else if (key == 11 && ((numericPopupContext == NumericPopupContext::M291Int && standardPopupM291IntMin < 0) ||
							(numericPopupContext == NumericPopupContext::M291Float && standardPopupM291FloatMin < 0.0f)))
						{
							String<32> temp;
							if (numericPopupM291Text.c_str()[0] == '-')
							{
								temp.copy(numericPopupM291Text.c_str() + 1);
							}
							else
							{
								temp.printf("-%s", numericPopupM291Text.c_str());
							}
							numericPopupM291Text.copy(temp.c_str());
							numericPopupM291Fresh = false;
						}
						RefreshM291NumericValue();
					}
					mgr.GetPopup()->Refresh(false);
				}
				currentButton.Clear();
				break;

			case evNumericBack:
				if (controlTempNumpadPopup != nullptr && mgr.GetPopup() == controlTempNumpadPopup)
				{
					if (numericPopupContext == NumericPopupContext::Temperature)
					{
						controlTempNumpadFresh = false;
						controlTempNumpadValue /= 10;
						RefreshControlTempNumpadValue();
					}
					else
					{
						const size_t len = numericPopupM291Text.strlen();
						if (len > 1)
						{
							numericPopupM291Text.Truncate(len - 1);
							numericPopupM291Fresh = false;
						}
						else
						{
							numericPopupM291Text.copy("0");
							numericPopupM291Fresh = true;
						}
						RefreshM291NumericValue();
					}
					mgr.GetPopup()->Refresh(false);
				}
				currentButton.Clear();
				break;

			case evNumericOk:
				if (controlTempNumpadPopup != nullptr && mgr.GetPopup() == controlTempNumpadPopup)
				{
					if (numericPopupContext == NumericPopupContext::Temperature)
					{
						const int value = static_cast<int>(controlTempNumpadValue);
						int minValue = 0;
						int maxValue = 0;
						if (ValidateControlTemperatureTarget(value, minValue, maxValue))
						{
							SendControlTemperatureTarget();
							mgr.ClearPopup();
						}
						else
						{
							mgr.ClearPopup();
							ShowControlTemperatureRangeAlert(minValue, maxValue);
						}
					}
					else if (numericPopupContext == NumericPopupContext::M291Int)
					{
						const bool syntaxValid = numericPopupM291Text.strlen() != 0 && strcmp(numericPopupM291Text.c_str(), "-") != 0;
						const int32_t value = syntaxValid ? StrToI32(numericPopupM291Text.c_str()) : 0;
						if (syntaxValid && value >= standardPopupM291IntMin && value <= standardPopupM291IntMax)
						{
							standardPopupM291ValueText.copy(numericPopupM291Text.c_str());
							standardPopupChoiceButtons[0]->SetText(standardPopupM291ValueText.c_str());
							numericPopupContext = NumericPopupContext::Temperature;
							mgr.ClearPopup();
							mgr.Refresh(false);
						}
						else
						{
							ShowM291NumericRangeError();
						}
					}
					else if (numericPopupContext == NumericPopupContext::M291Float)
					{
						const char * const txt = numericPopupM291Text.c_str();
						const bool syntaxValid = numericPopupM291Text.strlen() != 0 && strcmp(txt, "-") != 0 && strcmp(txt, ".") != 0 && strcmp(txt, "-.") != 0;
						const float value = syntaxValid ? SafeStrtof(txt) : 0.0f;
						if (syntaxValid && value >= standardPopupM291FloatMin && value <= standardPopupM291FloatMax)
						{
							standardPopupM291ValueText.copy(numericPopupM291Text.c_str());
							standardPopupChoiceButtons[0]->SetText(standardPopupM291ValueText.c_str());
							numericPopupContext = NumericPopupContext::Temperature;
							mgr.ClearPopup();
							mgr.Refresh(false);
						}
						else
						{
							ShowM291NumericRangeError();
						}
					}
				}
				currentButton.Clear();
				break;

			case evNumericCancel:
				if (controlTempNumpadPopup != nullptr && mgr.GetPopup() == controlTempNumpadPopup)
				{
					if (numericPopupContext != NumericPopupContext::Temperature)
					{
						numericPopupContext = NumericPopupContext::Temperature;
					}
					mgr.ClearPopup();
				}
				currentButton.Clear();
				break;

			case evControlMoveStep:
				{
					const int step = bp.GetIParam();
					if (step >= 0 && step < static_cast<int>(ControlMoveStepCount))
					{
						controlMoveSelectedStep = static_cast<unsigned int>(step);
						RefreshControlMoveSteps();
						mgr.Refresh(false);
					}
					currentButton.Clear();
				}
				break;

			case evControlMoveJog:
				{
					const int jog = bp.GetIParam();
					if (jog >= 0 && jog < 6)
					{
						const unsigned int axisSlot = static_cast<unsigned int>(jog / 2);
						OM::Axis * const axis = GetControlMoveAxis(axisSlot);
						if (axis == nullptr || !axis->homed)
						{
							String<64> message;
							message.printf("Printer %c AXIS not homed", "XYZ"[axisSlot]);
							ShowModernAlert(message.c_str());
						}
						else
						{
							const char sign = ((jog & 1) != 0) ? '+' : '-';
							SerialIo::Sendf("G91 G1 %c%c%s F%d G90\n", "XYZ"[axisSlot], sign,
								controlMoveStepText[controlMoveSelectedStep], nvData.GetFeedrate());
						}
					}
					currentButton.Clear();
				}
				break;

			case evControlMoveHome:
				{
					const int axisSlot = bp.GetIParam();
					if (axisSlot == 3)
					{
						// RRF dispatches bare G28 to sys/homeall.g.
						SerialIo::Sendf("G28\n");
					}
					else if (axisSlot >= 0 && axisSlot < static_cast<int>(ControlMoveAxisCount))
					{
						// RRF dispatches G28 X0/Y0/Z0 to homex.g/homey.g/homez.g.
						SerialIo::Sendf("G28 %c0\n", "XYZ"[axisSlot]);
					}
					currentButton.Clear();
				}
				break;

			case evControlMoveBedComp:
				// RRF executes the configured sys/bed.g sequence for G32.
				SerialIo::Sendf("G32\n");
				currentButton.Clear();
				break;

			case evControlExtrudeSpeed:
				{
					const int index = bp.GetIParam();
					if (index >= 0 && index < static_cast<int>(ControlExtrudeSpeedCount))
					{
						controlExtrudeSelectedSpeed = static_cast<unsigned int>(index);
						RefreshControlExtrudeSelections();
						mgr.Refresh(false);
					}
					currentButton.Clear();
				}
				break;

			case evControlExtrudeDistance:
				{
					const int index = bp.GetIParam();
					if (index >= 0 && index < static_cast<int>(ControlExtrudeDistanceCount))
					{
						controlExtrudeSelectedDistance = static_cast<unsigned int>(index);
						RefreshControlExtrudeSelections();
						mgr.Refresh(false);
					}
					currentButton.Clear();
				}
				break;

			case evControlExtrudeAction:
				SendControlExtrudeAction(bp.GetIParam() < 0);
				currentButton.Clear();
				break;

			case evControlExtrudePageUp:
				if (controlExtrudeToolPage > 0)
				{
					--controlExtrudeToolPage;
					RefreshControlExtrudeTools();
					mgr.Refresh(false);
				}
				currentButton.Clear();
				break;

			case evControlExtrudePageDown:
				{
					const unsigned int total = CountControlExtrudeTools();
					if ((controlExtrudeToolPage + 1) * ControlExtrudeToolsPerPage < total)
					{
						++controlExtrudeToolPage;
						RefreshControlExtrudeTools();
						mgr.Refresh(false);
					}
					currentButton.Clear();
				}
				break;
#endif

			case evStatusJobStatus:
#if DISPLAY_X == 800
				mgr.SetRoot(statusJobStatusRoot);
				RefreshJobStatusTiles();
				RefreshJobStatusHeader();
				RefreshJobStatusActions();
#else
				mgr.SetRoot(printRoot);
#endif
				currentUiPage = UiPage::StatusJobStatus;
				mgr.Refresh(true);
				currentButton.Clear();
				break;

#if DISPLAY_X == 800
			case evStatusJobStatusPauseResume:
				{
					const OM::PrinterStatus stat = GetStatus();
					if (stat == OM::PrinterStatus::printing)
					{
						OpenJobStatusConfirmation(JobStatusConfirmAction::Pause);
					}
					else if (stat == OM::PrinterStatus::paused)
					{
						OpenJobStatusConfirmation(JobStatusConfirmAction::Resume);
					}
					currentButton.Clear();
				}
				break;

			case evStatusJobStatusAbort:
				if (PrintInProgress())
				{
					OpenJobStatusConfirmation(JobStatusConfirmAction::Abort);
				}
				currentButton.Clear();
				break;

#endif

			case evStatusTune:
#if DISPLAY_X == 800
				mgr.SetRoot(statusTuneRoot);
				RefreshTunePage();
#else
				mgr.SetRoot(printRoot);
#endif
				currentUiPage = UiPage::StatusTune;
				mgr.Refresh(true);
				currentButton.Clear();
				break;

#if DISPLAY_X == 800
			case evTuneSpeed:
				OpenTuneSpeedPopup();
				currentButton.Clear();
				break;

			case evTuneGeneralFan:
				{
					const int slot = bp.GetIParam();
					if (slot >= 0 && slot < 2 && tuneGeneralFanIndices[slot] >= 0)
					{
						OpenTuneFanPopup(slot == 0 ? "FAN AUX" : "FAN CHAMBER", -1, tuneGeneralFanIndices[slot]);
					}
					currentButton.Clear();
				}
				break;

			case evTuneToolFan:
				{
					const int toolIndex = bp.GetIParam();
					OM::Tool * const tool = OM::GetTool(toolIndex);
					if (tool != nullptr && !tool->fans.IsEmpty())
					{
						String<24> title;
						title.printf("FAN T%d", toolIndex);
						OpenTuneFanPopup(title.c_str(), toolIndex, GetToolPartFan(tool));
					}
					currentButton.Clear();
				}
				break;

			case evTuneToolFlow:
				{
					const int toolIndex = bp.GetIParam();
					OM::Tool * const tool = OM::GetTool(toolIndex);
					if (tool != nullptr && !tool->extruders.IsEmpty())
					{
						OpenTuneFeedPopup(toolIndex, tool->extruders.LowestSetBit());
					}
					currentButton.Clear();
				}
				break;

			case evTunePressureAdvance:
				{
					const int toolIndex = bp.GetIParam();
					OM::Tool * const tool = OM::GetTool(toolIndex);
					if (tool != nullptr && !tool->extruders.IsEmpty())
					{
						OpenTunePressureAdvancePopup(toolIndex, tool->extruders.LowestSetBit());
					}
					currentButton.Clear();
				}
				break;

			case evTunePageUp:
				if (tuneToolPage > 0)
				{
					--tuneToolPage;
					RefreshTunePage();
					mgr.Refresh(false);
				}
				currentButton.Clear();
				break;

			case evTunePageDown:
				++tuneToolPage;
				RefreshTunePage();
				mgr.Refresh(false);
				currentButton.Clear();
				break;

			case evTuneZPlus:
				SerialIo::Sendf("M290 Z%s\n", babystepAmounts[nvData.GetBabystepAmountIndex()]);
				OM::IterateAxesWhile([](OM::Axis*& axis, size_t) {
					if (axis != nullptr && axis->letter[0] == 'Z')
					{
						axis->babystep += babystepAmountsF[nvData.GetBabystepAmountIndex()];
						return false;
					}
					return true;
				});
				RefreshTunePage();
				currentButton.Clear();
				break;

			case evTuneZMinus:
				SerialIo::Sendf("M290 Z-%s\n", babystepAmounts[nvData.GetBabystepAmountIndex()]);
				OM::IterateAxesWhile([](OM::Axis*& axis, size_t) {
					if (axis != nullptr && axis->letter[0] == 'Z')
					{
						axis->babystep -= babystepAmountsF[nvData.GetBabystepAmountIndex()];
						return false;
					}
					return true;
				});
				RefreshTunePage();
				currentButton.Clear();
				break;

			case evTunePopupAdjustPercent:
				tunePopupPercent = constrain<int>(tunePopupPercent + bp.GetIParam(), 0, 200);
				if (tunePopupKind == TunePopupKind::Fan)
				{
					tunePopupPercent = constrain<int>(tunePopupPercent, 0, 100);
				}
				UpdateTunePopupValue();
				mgr.GetPopup()->Refresh(false);
				currentButton.Clear();
				break;

			case evTunePopupAdjustPa:
				tunePopupPa += (float)bp.GetIParam() / 1000.0f;
				UpdateTunePopupValue();
				mgr.GetPopup()->Refresh(false);
				currentButton.Clear();
				break;

			case evTunePopupConfirm:
				switch (tunePopupKind)
				{
				case TunePopupKind::Speed:
					SerialIo::Sendf("M220 S%d\n", tunePopupPercent);
					break;
				case TunePopupKind::Fan:
					if (tunePopupResource >= 0)
					{
						SerialIo::Sendf("M106 P%d S%.3f\n", tunePopupResource, (double)tunePopupPercent / 100.0);
					}
					break;
				case TunePopupKind::Flow:
					if (tunePopupResource >= 0)
					{
						SerialIo::Sendf("M221 D%d S%d\n", tunePopupResource, tunePopupPercent);
					}
					break;
				case TunePopupKind::PressureAdvance:
					if (tunePopupResource >= 0)
					{
						SerialIo::Sendf("M572 D%d S%.4f\n", tunePopupResource, (double)tunePopupPa);
					}
					break;
				default:
					break;
				}
				mgr.ClearPopup();
				tunePopupKind = TunePopupKind::None;
				currentButton.Clear();
				break;

			case evTunePopupCancel:
				mgr.ClearPopup();
				tunePopupKind = TunePopupKind::None;
				currentButton.Clear();
				break;
#endif

			case evStatusJob:
#if DISPLAY_X == 800
				if (IsModernPageLockedByPrint(false))
				{
					mgr.Press(bp, false);
					currentButton.Clear();
					ShowModernAlert("JOB selection is unavailable while a print job is active.");
					break;
				}
				mgr.SetRoot(statusJobRoot);
				if (statusJobSdButton != nullptr)
				{
					mgr.Show(statusJobSdButton, statusJobNumVolumes > 1);
				}
				FileManager::DisplayFilesPage();
#else
				mgr.SetRoot(printRoot);
#endif
				currentUiPage = UiPage::StatusJob;
				mgr.Refresh(true);
				currentButton.Clear();
				break;

#if DISPLAY_X == 800
			case evStatusJobFile:
				{
					const char * const fileName = bp.GetSParam();
					if (fileName == nullptr)
					{
						ErrorBeep();
						break;
					}
					if (fileName[0] == '*')
					{
						FileManager::RequestFilesPageSubdir(fileName + 1);
					}
					else
					{
						currentFile = fileName;
						ConfigureStatusJobStartPopupContent();
						mgr.SetPopup(standardPopup, AutoPlace, AutoPlace);
					}
					currentButton.Clear();
				}
				break;

			case evStatusJobPageUp:
				mgr.Press(bp, false);
				currentButton.Clear();
				if (statusJobCanScrollEarlier)
				{
					FileManager::ScrollFilesPage(-static_cast<int>(StatusJobRows));
				}
				else if (statusJobInSubdir)
				{
					FileManager::RequestFilesPageParentDir();
				}
				currentButton.Clear();
				break;

			case evStatusJobPageDown:
				mgr.Press(bp, false);
				currentButton.Clear();
				if (statusJobCanScrollLater)
				{
					FileManager::ScrollFilesPage(static_cast<int>(StatusJobRows));
				}
				currentButton.Clear();
				break;

			case evStatusJobDeleteOpen:
				if (currentFile != nullptr)
				{
					ConfigureStatusJobDeletePopupContent();
					mgr.Refresh(true);
				}
				else
				{
					ErrorBeep();
				}
				currentButton.Clear();
				break;
#endif

			case evStatusObjects:
				mgr.SetRoot(statusObjectsRoot);
				currentUiPage = UiPage::StatusObjects;
#if DISPLAY_X == 800
				if (selectedStatusObject < 0 && currentStatusObject >= 0 &&
					currentStatusObject < static_cast<int>(statusObjectCount))
				{
					selectedStatusObject = currentStatusObject;
					statusObjectPage = static_cast<unsigned int>(currentStatusObject) / StatusObjectsPerPage;
				}
				RefreshStatusObjectsPage();
				statusObjectsDirty = false;
				statusObjectsNeedFullRefresh = false;
#endif
				mgr.Refresh(true);
				currentButton.Clear();
				break;

#if DISPLAY_X == 800
			case evStatusObjectSelect:
				{
					const unsigned int index = statusObjectPage * StatusObjectsPerPage + static_cast<unsigned int>(bp.GetIParam());
					SelectStatusObject(index, false);
					currentButton.Clear();
				}
				break;

			case evStatusObjectNumber:
				{
					const unsigned int index = statusObjectPage * StatusObjectsPerPage + static_cast<unsigned int>(bp.GetIParam());
					OpenStatusObjectCancelPopup(index);
					currentButton.Clear();
				}
				break;

			case evStatusObjectMarker:
				SelectStatusObject(static_cast<unsigned int>(bp.GetIParam()), true);
				currentButton.Clear();
				break;

			case evStatusObjectPageUp:
				if (statusObjectPage > 0)
				{
					--statusObjectPage;
					RefreshStatusObjectsPage();
					mgr.Refresh(true);
				}
				currentButton.Clear();
				break;

			case evStatusObjectPageDown:
				if ((statusObjectPage + 1) * StatusObjectsPerPage < statusObjectCount)
				{
					++statusObjectPage;
					RefreshStatusObjectsPage();
					mgr.Refresh(true);
				}
				currentButton.Clear();
				break;

#else
			case evStatusObject1:
				selectedStatusObject = statusObjectPage * StatusObjectsPerPage + 0;
				currentButton.Clear();
				break;
			case evStatusObject2:
				selectedStatusObject = statusObjectPage * StatusObjectsPerPage + 1;
				currentButton.Clear();
				break;
			case evStatusObject3:
				selectedStatusObject = statusObjectPage * StatusObjectsPerPage + 2;
				currentButton.Clear();
				break;
			case evStatusObject4:
				selectedStatusObject = statusObjectPage * StatusObjectsPerPage + 3;
				currentButton.Clear();
				break;
			case evStatusObject5:
				selectedStatusObject = statusObjectPage * StatusObjectsPerPage + 4;
				currentButton.Clear();
				break;
			case evStatusObject6:
				selectedStatusObject = statusObjectPage * StatusObjectsPerPage + 5;
				currentButton.Clear();
				break;
			case evStatusObjectPageUp:
				if (statusObjectPage > 0) --statusObjectPage;
				currentButton.Clear();
				break;
			case evStatusObjectPageDown:
				++statusObjectPage;
				currentButton.Clear();
				break;
#endif

			case evSystemConsole:
				if (currentTab != tabSystem)
				{
					ChangePage(tabSystem);
				}
				mgr.SetRoot(messageRoot);
				currentUiPage = UiPage::SystemConsole;
				mgr.Refresh(true);
				currentButton.Clear();
				break;

			case evSystemSettings:
				mgr.SetRoot(setupRoot);
				currentUiPage = UiPage::SystemSettings;
#if DISPLAY_X == 800
				RefreshModernSettingsPage();
#endif
				mgr.Refresh(true);
				currentButton.Clear();
				break;

#if DISPLAY_X == 800
			case evSettingsVolumeOpen:
				mgr.Press(bp, false); currentButton.Clear(); OpenSettingsVolumePopup(); break;

			case evStandardPopupChoice:
				switch (standardPopupContext)
				{
				case StandardPopupContext::SettingsVolume:
					settingsPendingVolume = bp.GetIParam();
					RefreshStandardPopupChoiceBorders(settingsPendingVolume);
					break;
				case StandardPopupContext::SettingsBrightness:
					settingsPendingBrightness = bp.GetIParam();
					RefreshStandardPopupChoiceBorders(settingsPendingBrightness);
					break;
				case StandardPopupContext::SettingsInfoTimeout:
					settingsPendingInfoTimeout = bp.GetIParam();
					RefreshStandardPopupChoiceBorders(settingsPendingInfoTimeout);
					break;
				case StandardPopupContext::SettingsBaud:
					settingsPendingBaud = bp.GetIParam();
					RefreshStandardPopupChoiceBorders(settingsPendingBaud);
					break;
				case StandardPopupContext::SettingsFeedrate:
					settingsPendingFeedrate = bp.GetIParam();
					RefreshStandardPopupChoiceBorders(settingsPendingFeedrate);
					break;
				case StandardPopupContext::SettingsBabystep:
					settingsPendingBabystepIndex = bp.GetIParam();
					RefreshStandardPopupChoiceBorders(settingsPendingBabystepIndex);
					break;
				case StandardPopupContext::SettingsAccent:
					settingsPendingAccent = bp.GetIParam();
					RefreshStandardPopupChoiceBorders(settingsPendingAccent);
					break;
				case StandardPopupContext::M291Choices:
					SerialIo::Sendf("M292 R{%d} S%lu\n", bp.GetIParam(), standardPopupM291Seq);
					ClearAlertOrResponse();
					break;
				case StandardPopupContext::M291ConfirmControls:
					if (bp.GetIParam() >= 1000 && bp.GetIParam() < 2000)
					{
						standardPopupM291SelectedAxis = bp.GetIParam() - 1000;
						OM::Axis * const axis = OM::GetAxis(standardPopupM291SelectedAxis);
						standardPopupM291SelectedAxisLetter = (axis != nullptr && axis->letter[0] != '\0') ? axis->letter[0] : '\0';
						RefreshModernM291AxisSelection();
					}
					else if (bp.GetIParam() >= 2000 && bp.GetIParam() < 2006 && standardPopupM291SelectedAxisLetter != '\0')
					{
						const size_t jog = (size_t)(bp.GetIParam() - 2000);
						SerialIo::Sendf("G91 G1 %s%c%s F%d G90\n",
							islower(standardPopupM291SelectedAxisLetter) ? "'" : "",
							standardPopupM291SelectedAxisLetter, standardPopupM291JogParam[jog], nvData.GetFeedrate());
					}
					break;
				case StandardPopupContext::M291NumberInt:
					OpenM291IntegerNumpad();
					break;
				case StandardPopupContext::M291NumberFloat:
					OpenM291FloatNumpad();
					break;
				case StandardPopupContext::M291Text:
					OpenM291TextKeyboard();
					break;
				default:
					break;
				}
				currentButton.Clear();
				break;

			case evStandardPopupConfirm:
				switch (standardPopupContext)
				{
				case StandardPopupContext::SettingsVolume:
					nvData.SetVolume(settingsPendingVolume);
					SaveSettings();
					TouchBeep();
					CloseStandardPopup();
					RefreshModernSettingsPage();
					break;
				case StandardPopupContext::SettingsBrightness:
					SetBrightness(settingsPendingBrightness);
					SaveSettings();
					CloseStandardPopup();
					RefreshModernSettingsPage();
					break;
				case StandardPopupContext::SettingsInfoTimeout:
					infoTimeout = settingsPendingInfoTimeout;
					nvData.SetInfoTimeout(infoTimeout);
					SaveSettings();
					CloseStandardPopup();
					RefreshModernSettingsPage();
					break;
				case StandardPopupContext::SettingsBaud:
					SetBaudRate(settingsPendingBaud);
					SaveSettings();
					CloseStandardPopup();
					RefreshModernSettingsPage();
					break;
				case StandardPopupContext::SettingsFeedrate:
					nvData.SetFeedrate((uint32_t)settingsPendingFeedrate);
					if (feedrateAmountButton != nullptr)
					{
						feedrateAmountButton->SetValue((uint32_t)settingsPendingFeedrate);
					}
					SaveSettings();
					CloseStandardPopup();
					RefreshModernSettingsPage();
					TouchBeep();
					break;
				case StandardPopupContext::SettingsBabystep:
					if (settingsPendingBabystepIndex >= 0 && settingsPendingBabystepIndex < (int)ARRAY_SIZE(babystepAmounts))
					{
						const uint32_t index = (uint32_t)settingsPendingBabystepIndex;
						nvData.SetBabystepAmountIndex(index);
						if (babystepAmountButton != nullptr) babystepAmountButton->SetText(babystepAmounts[index]);
						if (babystepMinusButton != nullptr) babystepMinusButton->SetText(babystepAmounts[index]);
						if (babystepPlusButton != nullptr) babystepPlusButton->SetText(babystepAmounts[index]);
						if (tuneZPlusButton != nullptr && tuneZMinusButton != nullptr)
						{
							tuneZPlusText.printf("+%s", babystepAmounts[index]);
							tuneZMinusText.printf("-%s", babystepAmounts[index]);
							tuneZPlusButton->SetText(tuneZPlusText.c_str());
							tuneZMinusButton->SetText(tuneZMinusText.c_str());
						}
						SaveSettings();
					}
					CloseStandardPopup();
					RefreshModernSettingsPage();
					TouchBeep();
					break;
				case StandardPopupContext::SettingsAccent:
					nvData.SetAccentColour((uint8_t)settingsPendingAccent);
					SaveSettings();
					CloseStandardPopup();
					currentButton.Clear();
					Reset();
					break;
				case StandardPopupContext::M291Confirm:
				case StandardPopupContext::M291ConfirmControls:
					SerialIo::Sendf("M292 P0 S%lu\n", standardPopupM291Seq);
					ClearAlertOrResponse();
					break;
				case StandardPopupContext::M291NumberInt:
					SerialIo::Sendf("M292 P0 R{%s} S%lu\n", standardPopupM291ValueText.c_str(), standardPopupM291Seq);
					ClearAlertOrResponse();
					break;
				case StandardPopupContext::M291NumberFloat:
					SerialIo::Sendf("M292 P0 R{%s} S%lu\n", standardPopupM291ValueText.c_str(), standardPopupM291Seq);
					ClearAlertOrResponse();
					break;
				case StandardPopupContext::M291Text:
					SerialIo::Sendf("M292 P0 R{\"%s\"} S%lu\n", standardPopupM291ValueText.c_str(), standardPopupM291Seq);
					ClearAlertOrResponse();
					break;
				case StandardPopupContext::SettingsTouchCalibration:
					CloseStandardPopup();
					currentButton.Clear();
					CalibrateTouch();
					SaveSettings();
					break;
				case StandardPopupContext::SettingsHeaterCombination:
					nvData.SetHeaterCombineType(nvData.GetHeaterCombineType() == HeaterCombineType::combined ? HeaterCombineType::notCombined : HeaterCombineType::combined);
					UI::AllToolsSeen();
					SaveSettings();
					CloseStandardPopup();
					RefreshModernSettingsPage();
					mgr.Refresh(false);
					break;
				case StandardPopupContext::SettingsFactoryReset:
					CloseStandardPopup();
					currentButton.Clear();
					FactoryReset();
					break;
				case StandardPopupContext::TuneSpeed:
					SerialIo::Sendf("M220 S%d\n", tunePopupPercent);
					tuneSpeedPercent = tunePopupPercent;
					CloseStandardPopup();
					tunePopupKind = TunePopupKind::None;
					tunePopupResource = -1;
					RefreshTunePage();
					mgr.Refresh(false);
					break;
				case StandardPopupContext::TuneFan:
					if (tunePopupResource >= 0)
					{
						SerialIo::Sendf("M106 P%d S%.3f\n", tunePopupResource, (double)tunePopupPercent / 100.0);
						if (tunePopupResource < (int)TuneMaxFans)
						{
							tuneFanPercent[tunePopupResource] = tunePopupPercent;
							tuneFanValid[tunePopupResource] = true;
						}
					}
					CloseStandardPopup();
					tunePopupKind = TunePopupKind::None;
					tunePopupResource = -1;
					RefreshTunePage();
					mgr.Refresh(false);
					break;
				case StandardPopupContext::TuneFlow:
					if (tunePopupResource >= 0)
					{
						SerialIo::Sendf("M221 D%d S%d\n", tunePopupResource, tunePopupPercent);
						if (tunePopupResource < (int)TuneMaxExtruders)
						{
							tuneExtruderFactor[tunePopupResource] = tunePopupPercent;
						}
					}
					CloseStandardPopup();
					tunePopupKind = TunePopupKind::None;
					tunePopupResource = -1;
					RefreshTunePage();
					mgr.Refresh(false);
					break;
				case StandardPopupContext::TunePressureAdvance:
					if (tunePopupResource >= 0)
					{
						SerialIo::Sendf("M572 D%d S%.4f\n", tunePopupResource, (double)tunePopupPa);
						if (tunePopupResource < (int)TuneMaxExtruders)
						{
							tunePressureAdvance[tunePopupResource] = tunePopupPa;
							tunePressureAdvanceValid[tunePopupResource] = true;
						}
					}
					CloseStandardPopup();
					tunePopupKind = TunePopupKind::None;
					tunePopupResource = -1;
					RefreshTunePage();
					mgr.Refresh(false);
					break;
				case StandardPopupContext::StatusObjectCancel:
					CloseStandardPopup();
					if (pendingStatusObjectCancel >= 0 &&
						pendingStatusObjectCancel < static_cast<int>(statusObjectCount) &&
						!statusObjects[pendingStatusObjectCancel].cancelled)
					{
						SerialIo::Sendf("M486 P%d\n", pendingStatusObjectCancel);
					}
					pendingStatusObjectCancel = -1;
					break;
				case StandardPopupContext::ControlMacroRun:
					CloseStandardPopup();
					if (controlMacroPendingFile.strlen() != 0)
					{
						SerialIo::Sendf("M98 P");
						SerialIo::SendFilename(CondStripDrive(FileManager::GetMacrosDir()), controlMacroPendingFile.c_str());
						SerialIo::SendChar('\n');
						controlMacroPendingFile.Clear();
					}
					break;
				case StandardPopupContext::JobStatusConfirm:
					switch (jobStatusConfirmAction)
					{
					case JobStatusConfirmAction::Pause:
							SerialIo::Sendf("M25\n");
						break;
					case JobStatusConfirmAction::Resume:
							SerialIo::Sendf("M24\n");
						break;
					case JobStatusConfirmAction::Abort:
						RequestJobAbort();
						break;
					default: break;
					}
					jobStatusConfirmAction = JobStatusConfirmAction::None;
					CloseStandardPopup();
					break;
				case StandardPopupContext::ControlHeaterOff:
					HandleControlHeaterOffConfirm();
					CloseStandardPopup();
					break;
				case StandardPopupContext::ControlToolChange:
					if (GetStatus() != OM::PrinterStatus::printing && GetStatus() != OM::PrinterStatus::simulating)
					{
						SerialIo::Sendf("T%d\n", controlToolChangeTarget);
					}
					CloseStandardPopup();
					break;
				case StandardPopupContext::StatusJobDelete:
					if (currentFile != nullptr)
					{
						SerialIo::Sendf("M30 ");
						SerialIo::SendFilename(CondStripDrive(StripPrefix(FileManager::GetFilesDir())), currentFile);
						SerialIo::SendChar('\n');
						FileManager::RefreshFilesList();
					}
					currentFile = nullptr;
					CloseStandardPopup();
					break;
				case StandardPopupContext::StatusJobStart:
					if (currentFile != nullptr)
					{
						SerialIo::Sendf("M32 ");
						SerialIo::SendFilename(CondStripDrive(StripPrefix(FileManager::GetFilesDir())), currentFile);
						SerialIo::SendChar('\n');
						PrintingFilenameChanged(currentFile);
						currentFile = nullptr;
						CurrentButtonReleased();
						PrintStarted();
					}
					CloseStandardPopup();
					break;
				default:
					CloseStandardPopup();
					break;
				}
				currentButton.Clear();
				break;

			case evStandardPopupCancel:
				if (standardPopupContext == StandardPopupContext::M291Confirm ||
					standardPopupContext == StandardPopupContext::M291ConfirmControls ||
					standardPopupContext == StandardPopupContext::M291Choices ||
					standardPopupContext == StandardPopupContext::M291NumberInt ||
					standardPopupContext == StandardPopupContext::M291NumberFloat ||
					standardPopupContext == StandardPopupContext::M291Text)
				{
					SerialIo::Sendf("M292 P1\n");
					ClearAlertOrResponse();
					currentButton.Clear();
					break;
				}
				if (standardPopupContext == StandardPopupContext::ModernInfo)
				{
					ClearAlertOrResponse();
					currentButton.Clear();
					break;
				}
				if (standardPopupContext == StandardPopupContext::StatusJobDelete && currentFile != nullptr)
				{
					ConfigureStatusJobStartPopupContent();
					mgr.Refresh(true);
					currentButton.Clear();
					break;
				}
				if (standardPopupContext == StandardPopupContext::TuneSpeed || standardPopupContext == StandardPopupContext::TuneFan || standardPopupContext == StandardPopupContext::TuneFlow || standardPopupContext == StandardPopupContext::TunePressureAdvance)
				{
					tunePopupKind = TunePopupKind::None;
					tunePopupResource = -1;
				}
				else if (standardPopupContext == StandardPopupContext::StatusObjectCancel)
				{
					pendingStatusObjectCancel = -1;
				}
				else if (standardPopupContext == StandardPopupContext::ControlMacroRun)
				{
					controlMacroPendingFile.Clear();
				}
				else if (standardPopupContext == StandardPopupContext::JobStatusConfirm)
				{
					jobStatusConfirmAction = JobStatusConfirmAction::None;
				}
				else if (standardPopupContext == StandardPopupContext::StatusJobStart)
				{
					currentFile = nullptr;
				}
				CloseStandardPopup();
				currentButton.Clear();
				break;

			case evSettingsBrightnessOpen:
				mgr.Press(bp, false); currentButton.Clear(); OpenSettingsBrightnessPopup(); break;
			case evSettingsInfoTimeoutOpen:
				mgr.Press(bp, false); currentButton.Clear(); OpenSettingsInfoTimeoutPopup(); break;

			case evSettingsAccentOpen:
				mgr.Press(bp, false); currentButton.Clear(); OpenSettingsAccentPopup(); break;

			case evSettingsAlwaysDimToggle:
				if (nvData.GetDisplayDimmerType() == DisplayDimmerType::always)
				{ nvData.SetDisplayDimmerType(DisplayDimmerType::never); }
				else
				{ nvData.SetDisplayDimmerType(DisplayDimmerType::always); }
				SaveSettings(); RefreshModernSettingsPage(); mgr.Press(bp, false); currentButton.Clear(); mgr.Refresh(false);
				if (nvData.GetDisplayDimmerType() == DisplayDimmerType::always)
				{
					DimDisplayNow();		// show the effect straight away instead of after the idle timeout; the next touch undoes it
				}
				break;

			case evSettingsBaudOpen:
				mgr.Press(bp, false); currentButton.Clear(); OpenSettingsBaudPopup(); break;

			case evSettingsTouchOpen:
				mgr.Press(bp, false); currentButton.Clear(); OpenSettingsTouchCalibrationPopup(); break;

			case evSettingsHeaterCombineOpen:
				mgr.Press(bp, false); currentButton.Clear(); OpenSettingsHeaterCombinationPopup(); break;

			case evSettingsFactoryResetOpen:
				mgr.Press(bp, false); currentButton.Clear(); OpenSettingsFactoryResetPopup(); break;

			case evSettingsPopupCancel:
				mgr.ClearPopup(); currentButton.Clear(); break;
#endif


			case evAdjustToolActiveTemp:
			case evAdjustToolStandbyTemp:
			case evAdjustBedActiveTemp:
			case evAdjustBedStandbyTemp:
			case evAdjustChamberActiveTemp:
			case evAdjustChamberStandbyTemp:
				if (setTempPopup == nullptr)
				{
					break;		// legacy adjust popup is not created in the 800x480 build
				}
				if (static_cast<IntegerButton*>(f)->GetValue() < 0)
				{
					static_cast<IntegerButton*>(f)->SetValue(0);
				}
				Adjusting(bp);
				if (isLandscape)
				{
					mgr.SetPopup(setTempPopup, AutoPlace, popupY);
				}
				break;

			case evAdjustActiveRPM:
				if (setRPMPopup == nullptr)
				{
					break;		// legacy RPM popup is not created in the 800x480 build
				}
				Adjusting(bp);
				if (isLandscape)
				{
					mgr.SetPopup(setRPMPopup, AutoPlace, popupY);
				}
				break;

			case evAdjustSpeed:
			case evExtrusionFactor:
			case evAdjustFan:
				if (setTempPopup == nullptr)
				{
					break;		// legacy adjust popup is not created in the 800x480 build
				}
				oldIntValue = static_cast<IntegerButton*>(bp.GetButton())->GetValue();
				Adjusting(bp);
				if (isLandscape)
				{
					mgr.SetPopup(setTempPopup, AutoPlace, popupY);
				}
				break;

			case evSetInt:
				if (fieldBeingAdjusted.IsValid())
				{
					int val = static_cast<const IntegerButton*>(fieldBeingAdjusted.GetButton())->GetValue();
					const event_t eventOfFieldBeingAdjusted = fieldBeingAdjusted.GetEvent();
					switch (eventOfFieldBeingAdjusted)
					{
					case evAdjustBedActiveTemp:
					case evAdjustChamberActiveTemp:
						{
							int index = fieldBeingAdjusted.GetIParam();
							const bool isBed = eventOfFieldBeingAdjusted == evAdjustBedActiveTemp;
							SerialIo::Sendf("%s P%d S%d\n", isBed ? "M140" : "M141", index, val);
						}
						break;

					case evAdjustBedStandbyTemp:
					case evAdjustChamberStandbyTemp:
						{
							int index = fieldBeingAdjusted.GetIParam();
							const bool isBed = eventOfFieldBeingAdjusted == evAdjustBedStandbyTemp;
							SerialIo::Sendf("%s P%d R%d\n", isBed ? "M140" : "M141", index, val);
						}
						break;

					case evAdjustToolActiveTemp:
						{
							int toolNumber = fieldBeingAdjusted.GetIParam();
							OM::Tool* tool = OM::GetTool(toolNumber);
							if (tool == nullptr)
							{
								break;
							}

							const bool useM568 = GetFirmwareFeatures().IsBitSet(m568TempAndRPM);
							if (nvData.GetHeaterCombineType() == HeaterCombineType::combined)
							{
								tool->UpdateTemp(0, val, true);
								SerialIo::Sendf("%s P%d S%d\n", (useM568 ? "M568" : "G10"), toolNumber, tool->heaters[0]->activeTemp);
							}
							else
							{

								// Find the slot for this button to determine which heater index it is
								{
									size_t slot = GetButtonSlot(activeTemps, fieldBeingAdjusted.GetButton());
									if (slot >= MaxSlots || (slot - tool->slot) >= MaxSlots)
									{
										break;
									}
									tool->UpdateTemp(slot - tool->slot, val, true);
								}

								String<maxUserCommandLength> heaterTemps;
								if (tool->GetHeaterTemps(heaterTemps.GetRef(), true))
								{
									SerialIo::Sendf("%s P%d S%s\n", (useM568 ? "M568" : "G10"), toolNumber, heaterTemps.c_str());
								}
							}
						}
						break;

					case evAdjustToolStandbyTemp:
						{
							int toolNumber = fieldBeingAdjusted.GetIParam();
							OM::Tool* tool = OM::GetTool(toolNumber);
							if (tool == nullptr)
							{
								break;
							}

							const bool useM568 = GetFirmwareFeatures().IsBitSet(m568TempAndRPM);
							if (nvData.GetHeaterCombineType() == HeaterCombineType::combined)
							{
								tool->UpdateTemp(0, val, false);
								SerialIo::Sendf("%s P%d R%d\n", (useM568 ? "M568" : "G10"), toolNumber, tool->heaters[0]->standbyTemp);
							}
							else
							{

								// Find the slot for this button to determine which heater index it is
								{
									size_t slot = GetButtonSlot(standbyTemps, fieldBeingAdjusted.GetButton());
									if (slot >= MaxSlots || (slot - tool->slot) >= MaxSlots)
									{
										break;
									}
									tool->UpdateTemp(slot - tool->slot, val, false);
								}

								String<maxUserCommandLength> heaterTemps;
								if (tool->GetHeaterTemps(heaterTemps.GetRef(), false))
								{
									SerialIo::Sendf("%s P%d R%s\n", (useM568 ? "M568" : "G10"), toolNumber, heaterTemps.c_str());
								}
							}
						}
						break;

					case evAdjustActiveRPM:
						{
							auto spindle = OM::GetSpindle(fieldBeingAdjusted.GetIParam());
							if (val == 0)
							{
								SerialIo::Sendf("M5 P%d\n", spindle->index);
							}
							else
							{
								SerialIo::Sendf("M%d P%d S%d\n", val < 0 ? 4 : 3, spindle->index, abs(val));
							}
						}
						break;

					case evExtrusionFactor:
						{
							const int extruder = fieldBeingAdjusted.GetIParam();
							SerialIo::Sendf("M221 D%d S%d\n", extruder, val);
						}
						break;

					case evAdjustFan:
						SerialIo::Sendf("M106 S%d\n", (256 * val)/100);
						break;

					default:
						{
							const char* null cmd = fieldBeingAdjusted.GetSParam();
							if (cmd != nullptr)
							{
								SerialIo::Sendf("%s%d\n", cmd, val);
							}
						}
						break;
					}
					mgr.ClearPopup();
					StopAdjusting();
				}
				break;

			case evAdjustInt:
				if (fieldBeingAdjusted.IsValid())
				{
					IntegerButton *ib = static_cast<IntegerButton*>(fieldBeingAdjusted.GetButton());
					const int change = bp.GetIParam();
					int newValue = ib->GetValue() + change;
					switch(fieldBeingAdjusted.GetEvent())
					{
					case evAdjustToolActiveTemp:
					case evAdjustToolStandbyTemp:
					case evAdjustBedActiveTemp:
					case evAdjustBedStandbyTemp:
					case evAdjustChamberActiveTemp:
					case evAdjustChamberStandbyTemp:
						newValue = constrain<int>(newValue, 0, 1600);		// some users want to print at high temperatures
						break;

					case evAdjustFan:
						newValue = constrain<int>(newValue, 0, 100);
						break;

					case evAdjustActiveRPM:
						{
							auto spindle = OM::GetSpindle(fieldBeingAdjusted.GetIParam());
							newValue = constrain<int>(newValue, -spindle->max, spindle->max);

							// If a change will lead us below the min speed for spindle skip to the other side
							if (newValue > (int)-spindle->min && newValue < (int)spindle->min)
							{
								newValue = (change < 0) ? -spindle->min : spindle->min;
							}
						}
						break;

					default:
						break;
					}
					ib->SetValue(newValue);
				}
				break;

			case evMovePopup:
				if (movePopup != nullptr)		// not created in the 800x480 build
				{
					mgr.SetPopup(movePopup, AutoPlace, AutoPlace);
				}
				break;

			case evMoveSelectAxis:
#if DISPLAY_X != 800
				alertPopup->ChangeLetter(bp.GetIParam());
#endif
				break;
			case evMoveAxis:
				{
					TextButtonForAxis *textButton = static_cast<TextButtonForAxis*>(bp.GetButton());
					const char letter = textButton->GetAxisLetter();
					SerialIo::Sendf("G91 G1 %s%c%s F%d G90\n", islower(letter) ? "'" : "", letter, bp.GetSParam(), nvData.GetFeedrate());
				}
				break;

			case evExtrudePopup:
				if (isLandscape && extrudePopup != nullptr)		// not created in the 800x480 build
				{
					mgr.SetPopup(extrudePopup, AutoPlace, AutoPlace);
				}
				break;

			case evExtrudeAmount:
				mgr.Press(currentExtrudeAmountPress, false);
				mgr.Press(bp, true);
				currentExtrudeAmountPress = bp;
				currentButton.Clear();						// stop it being released by the timer
				break;

			case evExtrudeRate:
				mgr.Press(currentExtrudeRatePress, false);
				mgr.Press(bp, true);
				currentExtrudeRatePress = bp;
				currentButton.Clear();						// stop it being released by the timer
				break;

			case evExtrude:
			case evRetract:
				if (currentExtrudeAmountPress.IsValid() && currentExtrudeRatePress.IsValid())
				{
					SerialIo::Sendf("M120 M83 G1 E%s%s F%s M121\n",
							(ev == evRetract ? "-" : ""),
							currentExtrudeAmountPress.GetSParam(),
							currentExtrudeRatePress.GetSParam());
				}
				break;

			case evBabyStepPopup:
				if (babystepPopup != nullptr)		// not created in the 800x480 build
				{
					mgr.SetPopup(babystepPopup, AutoPlace, AutoPlace);
				}
				break;

			case evBabyStepMinus:
			case evBabyStepPlus:
				if (babystepOffsetField != nullptr)		// only the legacy popup emits these events
				{
					SerialIo::Sendf("M290 Z%s%s\n", (ev == evBabyStepMinus ? "-" : ""), babystepAmounts[nvData.GetBabystepAmountIndex()]);
					float currentBabystepAmount = babystepOffsetField->GetValue();
					if (ev == evBabyStepMinus)
					{
						currentBabystepAmount -= babystepAmountsF[nvData.GetBabystepAmountIndex()];
					}
					else
					{
						currentBabystepAmount += babystepAmountsF[nvData.GetBabystepAmountIndex()];
					}
					babystepOffsetField->SetValue(currentBabystepAmount);
				}
				break;

			case evListFiles:
				FileManager::DisplayFilesList();
				break;

			case evListMacros:
				FileManager::DisplayMacrosList();
				break;

			case evCalTouch:
				CalibrateTouch();
				break;

			case evFactoryReset:
				PopupAreYouSure(ev, strings->confirmFactoryReset);
				break;

			case evSelectBed:
				{
					int bedIndex = bp.GetIParam();
					const OM::Bed* bed = OM::GetBed(bedIndex);
					if (bed == nullptr || bed->slot >= MaxSlots)
					{
						break;
					}
					const auto slot = bed->slot;
					if (bed->heaterStatus == OM::HeaterStatus::active)			// if bed is active
					{
						SerialIo::Sendf("M144 P%d\n", bedIndex);
					}
					else
					{
						SerialIo::Sendf("M140 P%d S%d\n", bedIndex, activeTemps[slot]->GetValue());
					}
				}
				break;

			case evSelectChamber:
				{
					const int chamberIndex = bp.GetIParam();
					const OM::Chamber* chamber = OM::GetChamber(chamberIndex);
					if (chamber == nullptr || chamber->slot >= MaxSlots)
					{
						break;
					}
					const auto slot = chamber->slot;
					SerialIo::Sendf("M141 P%d S%d\n",
							chamberIndex,
							(chamber->heaterStatus == OM::HeaterStatus::active ? -274 : activeTemps[slot]->GetValue()));
				}
				break;

			case evSelectHead:
				{
					int head = bp.GetIParam();
					// pressing a evSeelctHead button in the middle of active printing is almost always accidental (and fatal to the print job)
					if (GetStatus() != OM::PrinterStatus::printing && GetStatus() != OM::PrinterStatus::simulating)
					{
						if (head == currentTool)		// if head is active
						{
							SerialIo::Sendf("T-1\n");
						}
						else
						{
							SerialIo::Sendf("T%d\n", head);
						}
					}
				}
				break;

			case evFile:
				{
					const char * _ecv_array fileName = bp.GetSParam();
					if (fileName != nullptr)
					{
						if (fileName[0] == '*')
						{
							// It's a directory
							FileManager::RequestFilesSubdir(fileName + 1);
							//??? need to pop up a "wait" box here
						}
						else if (fileDetailPopup != nullptr)
						{
							// It's a regular file
							currentFile = fileName;
							FileSelected(currentFile);
							mgr.SetPopup(fileDetailPopup, AutoPlace, AutoPlace);
						}
						else
						{
							// No file info popup in the 800x480 build. Do not set currentFile, because that
							// would block file list updates (see IsDisplayingFileInfo).
							ErrorBeep();
						}
					}
					else
					{
						ErrorBeep();
					}
				}
				break;

			case evFilesUp:
				FileManager::RequestFilesParentDir();
				break;

			case evMacrosUp:
				FileManager::RequestMacrosParentDir();
				break;

			case evMacro:
			case evMacroControlPage:
				{
					const char *fileName = bp.GetSParam();
					if (fileName != nullptr)
					{
						if (fileName[0] == '*')		// if it's a directory
						{
							FileManager::RequestMacrosSubdir(fileName + 1);
							//??? need to pop up a "wait" box here
						}
						else
						{
							SerialIo::Sendf("M98 P");
							const char * _ecv_array const dir = (ev == evMacroControlPage) ? FileManager::GetMacrosRootDir() : FileManager::GetMacrosDir();
							SerialIo::SendFilename(CondStripDrive(dir), fileName);
							SerialIo::SendChar('\n');
						}
					}
					else
					{
						ErrorBeep();
					}
				}
				break;

			case evPrintFile:
			case evSimulateFile:
				mgr.ClearPopup();			// clear the file info popup
				mgr.ClearPopup();			// clear the file list popup
				if (currentFile != nullptr)
				{
					SerialIo::Sendf((ev == evSimulateFile) ? "M37 P" : "M32 ");
					SerialIo::SendFilename(CondStripDrive(StripPrefix(FileManager::GetFilesDir())), currentFile);
					SerialIo::SendChar('\n');
					PrintingFilenameChanged(currentFile);
					currentFile = nullptr;							// allow the file list to be updated
					CurrentButtonReleased();
					PrintStarted();
				}
				break;

			case evReprint:
			case evResimulate:
				if (lastJobFileNameAvailable)
				{
					SerialIo::Sendf("%s{job.lastFileName}\n", (ev == evResimulate) ? "M37 P" : "M32 ");
					CurrentButtonReleased();
					PrintStarted();
				}
				break;

			case evCancel:
				eventToConfirm = evNull;
				currentFile = nullptr;
				CurrentButtonReleased();
				PopupCancelled();
				mgr.ClearPopup();
				break;

			case evDeleteFile:
				CurrentButtonReleased();
				PopupAreYouSure(ev, strings->confirmFileDelete);
				break;

			case evSendCommand:
			case evPausePrint:
			case evResumePrint:
			case evReset:
				SerialIo::Sendf("%s\n", bp.GetSParam());
				break;

			case evHomeAxis:
				{
					const char letter = bp.GetSParam()[0];
					SerialIo::Sendf("G28 %s%c0\n", islower(letter) ? "'" : "", letter);
				}
				break;

			case evScrollFiles:
				FileManager::ScrollFiles(bp.GetIParam() * NumFileRows);
				break;

			case evScrollMacros:
				FileManager::ScrollMacros(bp.GetIParam() * NumMacroRows);
				break;

			case evChangeCard:
				(void)FileManager::NextCard();
				break;

			case evKeyboard:
				ShowKeyboard();
				break;

			case evInvertX:
				MirrorDisplay();
				CalibrateTouch();
#if DISPLAY_X == 800
				SaveSettings();
				currentButton.Clear();
#endif
				break;

			case evInvertY:
				InvertDisplay();
				CalibrateTouch();
#if DISPLAY_X == 800
				SaveSettings();
				currentButton.Clear();
#endif
				break;

			case evSetBaudRate:
				Adjusting(bp);
				mgr.SetPopup(baudPopup, AutoPlace, popupY);
				break;

			case evAdjustBaudRate:
				{
					const int rate = bp.GetIParam();
					SetBaudRate(rate);
					baudRateButton->SetValue(rate);
				}
				CurrentButtonReleased();
				mgr.ClearPopup();
				StopAdjusting();
				break;

			case evSetVolume:
				Adjusting(bp);
				mgr.SetPopup(volumePopup, AutoPlace, popupY);
				break;

			case evSetInfoTimeout:
				Adjusting(bp);
				mgr.SetPopup(infoTimeoutPopup, AutoPlace, popupY);
				break;

			case evSetScreensaverTimeout:
				Adjusting(bp);
				mgr.SetPopup(screensaverTimeoutPopup, AutoPlace, popupY);
				break;

			case evSetBabystepAmount:
#if DISPLAY_X == 800
				if (currentUiPage == UiPage::SystemSettings && IsModernPageLockedByPrint(false))
				{
					mgr.Press(bp, false);
					currentButton.Clear();
					ShowModernAlert("Z Offset step cannot be changed while a print job is active.");
					break;
				}
				if (currentUiPage == UiPage::SystemSettings)
				{
					mgr.Press(bp, false);
					currentButton.Clear();
					OpenSettingsBabystepPopup();
					break;
				}
#endif
				Adjusting(bp);
				mgr.SetPopup(babystepAmountPopup, AutoPlace, popupY);
				break;

			case evSetFeedrate:
#if DISPLAY_X == 800
				if (currentUiPage == UiPage::SystemSettings)
				{
					mgr.Press(bp, false);
					currentButton.Clear();
					OpenSettingsFeedratePopup();
					break;
				}
#endif
				Adjusting(bp);
				mgr.SetPopup(feedrateAmountPopup, AutoPlace, popupY);
				break;

			case evSetColours:
				if (coloursPopup != nullptr)
				{
					Adjusting(bp);
					mgr.SetPopup(coloursPopup, AutoPlace, popupY);
				}
				break;

			case evBrighter:
			case evDimmer:
				ChangeBrightness(ev == evBrighter);
				break;

			case evAdjustVolume:
				{
					const int newVolume = bp.GetIParam();
					nvData.SetVolume(newVolume);
					volumeButton->SetValue(newVolume);
				}
				TouchBeep();									// give audible feedback of the touch at the new volume level
				break;

			case evAdjustInfoTimeout:
				{
					infoTimeout = bp.GetIParam();
					nvData.SetInfoTimeout(infoTimeout);
					infoTimeoutButton->SetValue(infoTimeout);
				}
				TouchBeep();									// give audible feedback of the touch at the new volume level
				break;

			case evAdjustScreensaverTimeout:
				{
					uint32_t screensaverTimeout = bp.GetIParam();
					nvData.SetScreensaverTimeout(screensaverTimeout * 1000);
					screensaverTimeoutButton->SetValue(screensaverTimeout);
				}
				TouchBeep();									// give audible feedback of the touch at the new volume level
				break;

			case evAdjustBabystepAmount:
				{
					uint32_t babystepAmountIndex = bp.GetIParam();
					nvData.SetBabystepAmountIndex(babystepAmountIndex);
					if (babystepAmountButton != nullptr) babystepAmountButton->SetText(babystepAmounts[babystepAmountIndex]);
					if (babystepMinusButton != nullptr) babystepMinusButton->SetText(babystepAmounts[babystepAmountIndex]);
					if (babystepPlusButton != nullptr) babystepPlusButton->SetText(babystepAmounts[babystepAmountIndex]);
#if DISPLAY_X == 800
					if (tuneZPlusButton != nullptr && tuneZMinusButton != nullptr)
					{
						tuneZPlusText.printf("+%s", babystepAmounts[babystepAmountIndex]);
						tuneZMinusText.printf("-%s", babystepAmounts[babystepAmountIndex]);
						tuneZPlusButton->SetText(tuneZPlusText.c_str());
						tuneZMinusButton->SetText(tuneZMinusText.c_str());
					}
#endif
#if DISPLAY_X == 800
					RefreshModernSettingsPage();
					SaveSettings();
#endif
				}
				TouchBeep();									// give audible feedback of the touch at the new volume level
				break;

			case evAdjustFeedrate:
				{
					uint32_t feedrate = bp.GetIParam();
					nvData.SetFeedrate(feedrate);
					if (feedrateAmountButton != nullptr) feedrateAmountButton->SetValue(feedrate);
#if DISPLAY_X == 800
					RefreshModernSettingsPage();
					SaveSettings();
#endif
				}
				TouchBeep();									// give audible feedback of the touch at the new volume level
				break;

			case evAdjustColours:
				{
					const uint8_t newColours = (uint8_t)bp.GetIParam();
					if (nvData.SetColourScheme(newColours))
					{
						SaveSettings();
						Reset();
					}
				}
				mgr.ClearPopup();
				break;


			case evSetDimmingType:
				ChangeDisplayDimmerType();
				dimmingTypeButton->SetText(strings->displayDimmingNames[(unsigned int)nvData.GetDisplayDimmerType()]);
				break;

			case evSetHeaterCombineType:
				ChangeHeaterCombineType();
				heaterCombiningButton->SetText(strings->heaterCombineTypeNames[(unsigned int)nvData.GetHeaterCombineType()]);
				break;

			case evSetLogLevel:
				{
					MessageLog::LogLevel logLevel = MessageLog::LogLevelGet();

					logLevel = (MessageLog::LogLevel)(((int)logLevel + 1) % (int)MessageLog::LogLevel::NumTypes);

					logLevelButton->SetText(strings->logLevelNames[(unsigned int)logLevel]);

					nvData.SetLogLevel(logLevel);

					MessageLog::LogLevelSet(logLevel);
				}
				break;

			case evYes:
				CurrentButtonReleased();
				mgr.ClearPopup();								// clear the yes/no popup
				switch (eventToConfirm)
				{
				case evFactoryReset:
					FactoryReset();
					break;

				case evDeleteFile:
					if (currentFile != nullptr)
					{
						mgr.ClearPopup();						// clear the file info popup
						SerialIo::Sendf("M30 ");
						SerialIo::SendFilename(CondStripDrive(StripPrefix(FileManager::GetFilesDir())), currentFile);
						SerialIo::SendChar('\n');
						FileManager::RefreshFilesList();
						currentFile = nullptr;
					}
					break;

				default:
					break;
				}
				eventToConfirm = evNull;
				currentFile = nullptr;
				break;

			case evKey:
				if (!userCommandBuffers[currentUserCommandBuffer].cat((char)bp.GetIParam()))
				{
					userCommandField->SetChanged();
				}
				break;

			case evShift:
				{
					size_t rowOffset;
					if (keyboardShifted)
					{
						bp.GetButton()->Press(false, 0);
						rowOffset = 0;
					}
					else
					{
						rowOffset = 4;
					}
					for (size_t i = 0; i < 4; ++i)
					{
						keyboardRows[i]->ChangeText(currentKeyboard[i + rowOffset]);
					}
				}
				keyboardShifted = !keyboardShifted;
				currentButton.Clear();				// make the key sticky
				break;

			case evBackspace:
				if (!userCommandBuffers[currentUserCommandBuffer].IsEmpty())
				{
					userCommandBuffers[currentUserCommandBuffer].Erase(userCommandBuffers[currentUserCommandBuffer].strlen() - 1);
					userCommandField->SetChanged();
				}
				break;

			case evUp: // TODO new events for moving editor one left or right
				currentHistoryBuffer = (currentHistoryBuffer + numUserCommandBuffers - 1) % numUserCommandBuffers;
				if (currentHistoryBuffer == currentUserCommandBuffer)
				{
					userCommandBuffers[currentUserCommandBuffer].Clear();
				}
				else
				{
					userCommandBuffers[currentUserCommandBuffer].copy(userCommandBuffers[currentHistoryBuffer].c_str());
				}
				userCommandField->SetChanged();
				break;

			case evDown:
				currentHistoryBuffer = (currentHistoryBuffer + 1) % numUserCommandBuffers;
				if (currentHistoryBuffer == currentUserCommandBuffer)
				{
					userCommandBuffers[currentUserCommandBuffer].Clear();
				}
				else
				{
					userCommandBuffers[currentUserCommandBuffer].copy(userCommandBuffers[currentHistoryBuffer].c_str());
				}
				userCommandField->SetChanged();
				break;

			case evSendKeyboardCommand:
				{
#if DISPLAY_X == 800
					const bool isM291TextEntry = (keyboardDataHandler == M291TextData);
					const bool allowEmptyEntry = isM291TextEntry && standardPopupM291TextMin == 0;
#else
					const bool isM291TextEntry = false;
					const bool allowEmptyEntry = false;
#endif
					const bool hasText = userCommandBuffers[currentUserCommandBuffer].strlen() != 0;
					if (hasText || allowEmptyEntry)
					{
						if (keyboardDataHandler)
						{
							keyboardDataHandler(userCommandBuffers[currentUserCommandBuffer].c_str());
						}

						// Console commands keep their history behavior. M291 text editing is transient.
						if (!isM291TextEntry)
						{
							size_t prevBuffer = (currentUserCommandBuffer + numUserCommandBuffers - 1) % numUserCommandBuffers;
							if (strcmp(userCommandBuffers[currentUserCommandBuffer].c_str(), userCommandBuffers[prevBuffer].c_str()) != 0)
							{
								currentUserCommandBuffer = (currentUserCommandBuffer + 1) % numUserCommandBuffers;
							}
						}
						currentHistoryBuffer = currentUserCommandBuffer;
						userCommandBuffers[currentUserCommandBuffer].Clear();
						userCommandField->SetLabel(userCommandBuffers[currentUserCommandBuffer].c_str());
					}
				}
				break;

			case evOkAlert:
#if DISPLAY_X != 800
				alertPopup->ProcessOkButton();
#endif
				break;

			case evCloseAlert:
				SerialIo::Sendf("%s\n", bp.GetSParam());
				break;

			case evChoiceAlert:
#if DISPLAY_X != 800
				alertPopup->ProcessChoice(bp.GetIParam());
#endif
				break;

			case evEditAlert:
#if DISPLAY_X != 800
				keyboardDataHandler = PopupEditData;
				mgr.SetPopup(keyboardPopup, AutoPlace, keyboardPopupY);
				keyboardIsDisplayed = true;
#endif
				break;

			default:
				break;
			}
#if DISPLAY_X == 800
			// Many modern actions switch roots or open/close popups immediately.
			// Release the button object that was actually touched so its accent
			// pressed fill cannot remain stuck on an old root/popup.
			if (!KeepModernButtonHighlighted(f))
			{
				mgr.Press(bp, false);
			}
			SyncTopTabHighlight();
#endif
		}
	}

	// Process a touch event outside the popup on the field being adjusted
	void ProcessTouchOutsidePopup(ButtonPress bp)
	{
		if (!IsSetupTab())
		{
			return;
		}

		if (bp == fieldBeingAdjusted)
		{
			TouchBeep();
			switch(fieldBeingAdjusted.GetEvent())
			{
			case evAdjustSpeed:
			case evExtrusionFactor:
			case evAdjustFan:
				static_cast<IntegerButton*>(fieldBeingAdjusted.GetButton())->SetValue(oldIntValue);
				mgr.ClearPopup();
				StopAdjusting();
				break;

			case evAdjustToolActiveTemp:
			case evAdjustToolStandbyTemp:
			case evAdjustBedActiveTemp:
			case evAdjustBedStandbyTemp:
			case evAdjustChamberActiveTemp:
			case evAdjustChamberStandbyTemp:
			case evAdjustActiveRPM:
			case evSetBaudRate:
			case evSetVolume:
			case evSetInfoTimeout:
			case evSetScreensaverTimeout:
			case evSetFeedrate:
			case evSetBabystepAmount:
			case evSetColours:
				mgr.ClearPopup();
				StopAdjusting();
				break;
			}
		}
		else
		{
			switch(bp.GetEvent())
			{
			case evEmergencyStop:
				mgr.Press(bp, true);
				DoEmergencyStop();
				mgr.Press(bp, false);
				break;

			case evTabControl:
			case evTabStatus:
			case evTabMsg:
			case evTabSetup:
				StopAdjusting();
				TouchBeep();
				{
					ButtonBase *btn = bp.GetButton();
					if (ChangePage(btn))
					{
						currentButton.Clear();						// keep the button highlighted after it is released
					}
				}
				break;

			case evSetBaudRate:
			case evSetVolume:
			case evSetInfoTimeout:
			case evSetScreensaverTimeout:
			case evSetFeedrate:
			case evSetBabystepAmount:
			case evSetColours:
			case evCalTouch:
			case evInvertX:
			case evInvertY:
			case evFactoryReset:
				// On the Setup tab, we allow any other button to be pressed to exit the current popup
				StopAdjusting();
				mgr.ClearPopup();
				ProcessTouch(bp);
				break;

			default:
				break;
			}
		}
	}

	// This is called when a button press times out
	void OnButtonPressTimeout()
	{
		if (currentButton.IsValid())
		{
			CurrentButtonReleased();
		}
	}

	void DisplayFilesPopup(int cardNumber, unsigned int numMountedVolumes)
	{
#if DISPLAY_X == 800
		// The modern 800x480 UI owns the embedded STATUS > JOB browser. Never
		// open the legacy file-list popup when changing storage volumes.
		UNUSED(cardNumber);
		statusJobNumVolumes = numMountedVolumes;
		if (statusJobSdButton != nullptr)
		{
			mgr.Show(statusJobSdButton, numMountedVolumes > 1);
		}
		return;
#else
		filePopupTitleField->SetValue(cardNumber);
		mgr.Show(changeCardButton, numMountedVolumes > 1);

		if (isLandscape)
		{
			for (size_t i = 0; i < ARRAY_SIZE(filenameButtons); i++)
			{
				filenameButtons[i]->Press(false, 0);
				filenameButtons[i]->Show(false);
			}
			fileListPopupNoFiles->Show(true);
			mgr.SetPopup(fileListPopup, AutoPlace, AutoPlace);
		}
#endif
	}

	void FileListCardButtonUpdate(unsigned int numMountedVolumes)
	{
#if DISPLAY_X == 800
		statusJobNumVolumes = numMountedVolumes;
		if (statusJobSdButton != nullptr)
		{
			mgr.Show(statusJobSdButton, numMountedVolumes > 1);
		}
#else
		mgr.Show(changeCardButton, numMountedVolumes > 1);
#endif
	}

	void DisplayMacrosPopup()
	{
		if (isLandscape)
		{
			for (size_t i = 0; i < ARRAY_SIZE(macroButtons); i++)
			{
				macroButtons[i]->Press(false, 0);
				macroButtons[i]->Show(false);
			}
			mgr.SetPopup(macrosPopup, AutoPlace, AutoPlace);
		}
	}

	void FileListLoaded(bool filesNotMacros, int errCode)
	{
		FileListButtons& buttons = (filesNotMacros) ? filesListButtons : macrosListButtons;
		if (errCode == 0)
		{
			mgr.Show(buttons.errorField, false);
		}
		else
		{
			buttons.errorField->SetValue(errCode);
			mgr.Show(buttons.errorField, true);
		}
	}

	void EnableFileNavButtons(bool filesNotMacros, bool scrollEarlier, bool scrollLater, bool parentDir)
	{
		FileListButtons& buttons = (filesNotMacros) ? filesListButtons : macrosListButtons;
		mgr.Show(buttons.scrollLeftButton, scrollEarlier);
		mgr.Show(buttons.scrollRightButton, scrollLater);
		mgr.Show(buttons.folderUpButton, parentDir);
	}

	// Update the specified button in the file or macro buttons list. If 'text' is nullptr then hide the button, else display it.
	void EnableStatusJobNavButtons(bool scrollEarlier, bool scrollLater, bool parentDir)
	{
#if DISPLAY_X == 800
		statusJobCanScrollEarlier = scrollEarlier;
		statusJobCanScrollLater = scrollLater;
		statusJobInSubdir = parentDir;

		// A page arrow may disappear as a direct result of the press that changed
		// pages. Clear its pressed state and touch event before hiding it so no
		// white pressed outline can remain on the last/first page.
		if (statusJobPageUpButton != nullptr)
		{
			const bool showUp = scrollEarlier || parentDir;
			statusJobPageUpButton->Press(false, 0);
			statusJobPageUpButton->SetEvent(showUp ? evStatusJobPageUp : evNull, 0);
			mgr.Show(statusJobPageUpButton, showUp);
		}
		if (statusJobPageDownButton != nullptr)
		{
			statusJobPageDownButton->Press(false, 0);
			statusJobPageDownButton->SetEvent(scrollLater ? evStatusJobPageDown : evNull, 0);
			mgr.Show(statusJobPageDownButton, scrollLater);
		}
#else
		UNUSED(scrollEarlier);
		UNUSED(scrollLater);
		UNUSED(parentDir);
#endif
	}

	void UpdateStatusJobFileButton(unsigned int buttonIndex, const char * _ecv_array null text, const char * _ecv_array null param)
	{
#if DISPLAY_X == 800
		if (buttonIndex >= StatusJobRows || statusJobFileButtons[buttonIndex] == nullptr)
		{
			return;
		}

		ModernTextButton * const button = statusJobFileButtons[buttonIndex];
		const bool hasEntry = (text != nullptr && text[0] != 0);
		const bool isDirectory = (hasEntry && text[0] == '*');

		// FileManager may supply an empty string for an unused row. Treat that
		// exactly like nullptr: no tile, no event and no retained pressed outline.
		if (!hasEntry)
		{
			button->Press(false, 0);
			button->SetText(nullptr);
			button->SetEvent(evNull, static_cast<const char *>(nullptr));
			button->SetBorderVisible(false);
			mgr.Show(button, false);
			return;
		}

		button->SetText(isDirectory ? text + 1 : text);
		button->SetEvent(evStatusJobFile, param);
		button->SetBorderVisible(isDirectory);
		if (isDirectory)
		{
			button->SetBorderColour(UTFT::fromRGB(59, 67, 79));
		}
		mgr.Show(button, true);
#else
		UNUSED(buttonIndex);
		UNUSED(text);
		UNUSED(param);
#endif
	}

	void EnableControlMacroNavButtons(bool scrollEarlier, bool scrollLater, bool parentDir)
	{
#if DISPLAY_X == 800
		controlMacroCanScrollEarlier = scrollEarlier;
		controlMacroCanScrollLater = scrollLater;
		controlMacroInSubdir = parentDir;

		if (controlMacroPageUpButton != nullptr)
		{
			const bool showUp = scrollEarlier || parentDir;
			controlMacroPageUpButton->Press(false, 0);
			controlMacroPageUpButton->SetEvent(showUp ? evControlMacroPageUp : evNull, 0);
			mgr.Show(controlMacroPageUpButton, showUp);
		}
		if (controlMacroPageDownButton != nullptr)
		{
			controlMacroPageDownButton->Press(false, 0);
			controlMacroPageDownButton->SetEvent(scrollLater ? evControlMacroPageDown : evNull, 0);
			mgr.Show(controlMacroPageDownButton, scrollLater);
		}
#else
		UNUSED(scrollEarlier);
		UNUSED(scrollLater);
		UNUSED(parentDir);
#endif
	}

	void UpdateControlMacroFileButton(unsigned int buttonIndex, const char * _ecv_array null text, const char * _ecv_array null param)
	{
#if DISPLAY_X == 800
		if (buttonIndex >= ControlMacroRows || controlMacroFileButtons[buttonIndex] == nullptr)
		{
			return;
		}

		ModernTextButton * const button = controlMacroFileButtons[buttonIndex];
		const bool hasEntry = (text != nullptr && text[0] != 0);
		const bool isDirectory = (hasEntry && text[0] == '*');

		if (!hasEntry)
		{
			button->Press(false, 0);
			button->SetText(nullptr);
			button->SetEvent(evNull, static_cast<const char *>(nullptr));
			button->SetBorderVisible(false);
			mgr.Show(button, false);
			return;
		}

		const char *displayText = isDirectory ? text + 1 : text;
		displayText = SkipDigitsAndUnderscore(displayText);
		button->SetText(displayText);
		button->SetEvent(evControlMacroFile, param);
		button->SetBorderVisible(isDirectory);
		if (isDirectory)
		{
			button->SetBorderColour(UTFT::fromRGB(59, 67, 79));
		}
		mgr.Show(button, true);
#else
		UNUSED(buttonIndex);
		UNUSED(text);
		UNUSED(param);
#endif
	}

	void UpdateFileButton(bool filesNotMacros, unsigned int buttonIndex, const char * _ecv_array null text, const char * _ecv_array null param)
	{
		if (filesNotMacros && text)
		{
			fileListPopupNoFiles->Show(false);
		}

		if (buttonIndex < ((filesNotMacros) ? NumDisplayedFiles : NumDisplayedMacros))
		{
			TextButton * const f = ((filesNotMacros) ? filenameButtons : macroButtons)[buttonIndex];
			f->SetText(text);
			f->SetEvent((text == nullptr) ? evNull : (filesNotMacros) ? evFile : evMacro, param);
			mgr.Show(f, text != nullptr);
		}
	}

	// Update the specified button in the macro short list. If 'fileName' is nullptr then hide the button, else display it.
	// Return true if this should be called again for the next button.
	bool UpdateMacroShortList(unsigned int buttonIndex, const char * _ecv_array null fileName)
	{
#if (DISPLAY_X == 480)
		const bool tooFewSpace = numToolColsUsed >= (MaxSlots - 1);
#else
		const bool tooFewSpace = numToolColsUsed > (MaxSlots - 2);
#endif

		if (buttonIndex >= ARRAY_SIZE(controlPageMacroButtons) || numToolColsUsed == 0 || tooFewSpace)
		{
			return false;
		}

		String<controlPageMacroTextLength>& str = controlPageMacroText[buttonIndex];
		str.Clear();
		const bool isFile = (fileName != nullptr);
		if (isFile)
		{
			str.copy(fileName);
		}
		TextButton * const f = controlPageMacroButtons[buttonIndex];
		f->SetText(SkipDigitsAndUnderscore(str.c_str()));
		f->SetEvent((isFile) ? evMacroControlPage : evNull, str.c_str());
		mgr.Show(f, isFile);
		return true;
	}

	unsigned int GetNumScrolledFiles(bool filesNotMacros)
	{
		return (filesNotMacros) ? NumFileRows : NumMacroRows;
	}

	void AdjustControlPageMacroButtons()
	{
		const unsigned int n = numToolColsUsed;

		if (n != numHeaterAndToolColumns)
		{
			numHeaterAndToolColumns = n;

			// Adjust the width of the control page macro buttons, or hide them completely if insufficient room
			PixelNumber controlPageMacroButtonsColumn = (PixelNumber)(((tempButtonWidth + fieldSpacing) * n) + bedColumn + fieldSpacing);
			PixelNumber controlPageMacroButtonsWidth = (PixelNumber)((controlPageMacroButtonsColumn >= DisplayX - margin) ? 0 : DisplayX - margin - controlPageMacroButtonsColumn);
			if (controlPageMacroButtonsWidth > maxControlPageMacroButtonsWidth)
			{
				controlPageMacroButtonsColumn += controlPageMacroButtonsWidth - maxControlPageMacroButtonsWidth;
				controlPageMacroButtonsWidth = maxControlPageMacroButtonsWidth;
			}

			bool showControlPageMacroButtons = controlPageMacroButtonsWidth >= minControlPageMacroButtonsWidth;

			for (TextButton *& b : controlPageMacroButtons)
			{
				if (showControlPageMacroButtons)
				{
					b->SetPositionAndWidth(controlPageMacroButtonsColumn, controlPageMacroButtonsWidth);
				}
				mgr.Show(b, showControlPageMacroButtons);
			}

			if (currentTab == tabControl)
			{
				mgr.Refresh(true);
			}
		}
	}

	void ResetToolAndHeaterStates() noexcept
	{
		for (size_t i = 0; i < numToolColsUsed; ++i)
		{
			toolButtons[i]->SetColours(colours->buttonTextColour, colours->buttonImageBackColour);
			currentTemps[i]->SetColours(colours->infoTextColour, colours->defaultBackColour);
		}
	}

	void ManageCurrentActiveStandbyFields(
			size_t& slot,
			const bool showCurrent = false,
			const Event activeEvent = evNull,
			const int activeEventValue = -1,
			const Event standbyEvent = evNull,
			const int standbyEventValue = -1
			)
	{
		mgr.Show(currentTemps[slot], showCurrent);
		mgr.Show(activeTemps[slot], activeEvent != evNull);
		mgr.Show(standbyTemps[slot], standbyEvent != evNull);

		activeTemps[slot]->SetEvent(activeEvent, activeEventValue);
		activeTemps[slot]->SetValue(0);
		standbyTemps[slot]->SetEvent(standbyEvent, standbyEventValue);
		standbyTemps[slot]->SetValue(0);
	}

	size_t AddBedOrChamber(OM::BedOrChamber *bedOrChamber, size_t &slot, const bool isBed = true) {
		const size_t count = (isBed ? OM::GetBedCount() : OM::GetChamberCount());
		bedOrChamber->slot = MaxSlots;
		if (slot < MaxSlots && bedOrChamber->heater > -1) {
			bedOrChamber->slot = slot;
			mgr.Show(toolButtons[slot], true);
			ManageCurrentActiveStandbyFields(
					slot,
					true,
					isBed ? evAdjustBedActiveTemp : evAdjustChamberActiveTemp,
					bedOrChamber->index,
					isBed ? evAdjustBedStandbyTemp : evAdjustChamberStandbyTemp,
					bedOrChamber->index
					);
			mgr.Show(extrusionFactors[slot], false);
			toolButtons[slot]->SetEvent(isBed ? evSelectBed : evSelectChamber, bedOrChamber->index);
			toolButtons[slot]->SetIcon(isBed ? IconBed : IconChamber);
			toolButtons[slot]->SetIntVal(bedOrChamber->index);
			toolButtons[slot]->SetPrintText(count > 1);

			++slot;
		}
		return count;
	}

	void AllToolsSeen()
	{
		size_t slot = 0;
		size_t bedCount = 0;
		size_t chamberCount = 0;
		auto firstBed = OM::GetFirstBed();
		if (firstBed != nullptr)
		{
			bedCount = AddBedOrChamber(firstBed, slot);
		}
		OM::IterateToolsWhile([&slot](OM::Tool*& tool, size_t)
		{
			tool->slot = slot;
			const bool hasHeater = tool->heaters[0] != nullptr;
			const bool hasSpindle = tool->spindle != nullptr;
			const bool hasExtruder = tool->extruders.IsNonEmpty();
			if (slot < MaxSlots)
			{
				toolButtons[slot]->SetEvent(evSelectHead, tool->index);
				toolButtons[slot]->SetIntVal(tool->index);
				toolButtons[slot]->SetPrintText(true);
				toolButtons[slot]->SetIcon(hasSpindle ? IconSpindle : IconNozzle);
				mgr.Show(toolButtons[slot], true);

				mgr.Show(extrusionFactors[slot], hasExtruder);
				if (hasExtruder)
				{
					extrusionFactors[slot]->SetEvent(extrusionFactors[slot]->GetEvent(), (int) tool->extruders.LowestSetBit());
				}

				// Spindle takes precedence
				if (hasSpindle)
				{
					ManageCurrentActiveStandbyFields(slot, true, evAdjustActiveRPM, tool->spindle->index);
					++slot;
				}
				else if (hasHeater)
				{
					if (nvData.GetHeaterCombineType() == HeaterCombineType::notCombined)
					{
						tool->IterateHeaters([&slot, &tool](OM::ToolHeater*, size_t)
						{
							// only one heater per slot can be displayed
							if (slot >= MaxSlots)
							{
								return;
							}
							ManageCurrentActiveStandbyFields(
									slot,
									true,
									evAdjustToolActiveTemp, tool->index,
									evAdjustToolStandbyTemp, tool->index);
							++slot;
						});
					}
					else
					{
						ManageCurrentActiveStandbyFields(
								slot,
								true,
								evAdjustToolActiveTemp, tool->index,
								evAdjustToolStandbyTemp, tool->index);
						++slot;
					}
				}
				else
				{
					// Hides everything by default
					ManageCurrentActiveStandbyFields(slot);
					++slot;
				}
			}
			return slot < MaxSlots;
		});
		auto firstChamber = OM::GetFirstChamber();
		if (firstChamber != nullptr)
		{
			chamberCount = AddBedOrChamber(firstChamber, slot, false);
		}

		// Fill remaining space with additional beds
		if (slot < MaxSlots && bedCount > 1)
		{
			OM::IterateBedsWhile([&slot](OM::Bed*& bed, size_t) {
				AddBedOrChamber(bed, slot);
				return slot < MaxSlots;
			}, 1);
		}

		// Fill remaining space with additional chambers
		if (slot < MaxSlots && chamberCount > 1)
		{
			OM::IterateChambersWhile([&slot](OM::Chamber*& chamber, size_t) {
				AddBedOrChamber(chamber, slot, false);
				return slot < MaxSlots;
			}, 1);
		}

		numToolColsUsed = slot;
		for (size_t i = slot; i < MaxSlots; ++i)
		{
			mgr.Show(toolButtons[i], false);
			mgr.Show(currentTemps[i], false);
			mgr.Show(activeTemps[i], false);
			mgr.Show(standbyTemps[i], false);
			mgr.Show(extrusionFactors[i], false);
		}
		ResetToolAndHeaterStates();
		AdjustControlPageMacroButtons();
#if DISPLAY_X == 800
		if (controlToolHeaderCards[0] != nullptr)
		{
			RefreshControlToolsPage();
		}
		if (controlExtrudeActiveToolCard != nullptr)
		{
			SelectControlExtrudePageForActiveTool();
			RefreshControlExtrudeTools();
		}
#endif
	}

	void SetSpindleActive(size_t spindleIndex, int32_t activeRpm)
	{
		auto spindle = OM::GetOrCreateSpindle(spindleIndex);
		if (spindle == nullptr)
		{
			return;
		}
		spindle->active = abs(activeRpm);
		if (!GetFirmwareFeatures().IsBitSet(m568TempAndRPM))
		{
			if (activeRpm == 0)
			{
				spindle->state = OM::SpindleState::stopped;
			}
			else if (activeRpm > 0)
			{
				spindle->state = OM::SpindleState::forward;
			}
			else
			{
				spindle->state = OM::SpindleState::reverse;
			}
		}

		OM::IterateToolsWhile([spindle](OM::Tool*& tool, size_t) {
			if (tool->slot < MaxSlots && tool->spindle == spindle)
			{
				activeTemps[tool->slot]->SetValue(tool->spindle->active);
			}
			return tool->slot < MaxSlots;
		});
	}

	void UpdateSpindleCurrent(OM::Spindle* spindle)
	{
		OM::IterateToolsWhile([spindle](OM::Tool*& tool, size_t) {
			if (tool->slot < MaxSlots && tool->spindle == spindle)
			{
				const OM::SpindleState state = spindle->state;
				currentTemps[tool->slot]->SetValue(
						(state == OM::SpindleState::stopped)
							? 0
							: (state == OM::SpindleState::forward)
							  	  ? spindle->current
							  	  : -spindle->current);
			}
			return tool->slot < MaxSlots;
		});
	}

	void SetSpindleCurrent(size_t spindleIndex, int32_t current)
	{
		auto spindle = OM::GetOrCreateSpindle(spindleIndex);
		if (spindle == nullptr)
		{
			return;
		}
		spindle->current = abs(current);
		if (!GetFirmwareFeatures().IsBitSet(m568TempAndRPM))
		{
			if (current == 0)
			{
				spindle->state = OM::SpindleState::stopped;
			}
			else if (current > 0)
			{
				spindle->state = OM::SpindleState::forward;
			}
			else
			{
				spindle->state = OM::SpindleState::reverse;
			}
		}
		UpdateSpindleCurrent(spindle);
	}

	void SetSpindleLimit(size_t spindleIndex, uint32_t value, bool max)
	{
		OM::Spindle *spindle = OM::GetOrCreateSpindle(spindleIndex);
		if (spindle != nullptr)
		{
			if (max)
			{
				spindle->max = value;
			}
			else
			{
				spindle->min = value;
			}
		}
	}

	void SetSpindleState(size_t spindleIndex, OM::SpindleState state)
	{
		OM::Spindle* spindle = OM::GetOrCreateSpindle(spindleIndex);
		if (spindle == nullptr)
		{
			return;
		}
		const bool changed = spindle->state != state;
		spindle->state = state;
		if (changed)
		{
			UpdateSpindleCurrent(spindle);
		}
	}

	// This handles the old path where tools were assigned to spindles
	void SetSpindleTool(int8_t spindleNumber, int8_t toolIndex)
	{
		auto sp = OM::GetOrCreateSpindle(spindleNumber);
		if (sp == nullptr)
		{
			return;
		}
		if (toolIndex == -1)
		{
			OM::IterateToolsWhile([sp](OM::Tool*& tool, size_t) {
				if (tool->spindle == sp)
				{
					tool->spindle = nullptr;
				}
				return true;
			});
		}
		else
		{
			OM::Tool *tool = OM::GetOrCreateTool(toolIndex);
			if (tool != nullptr)
			{
				tool->spindle = sp;
			}
		}
	}

	void UpdateToolStatus(size_t toolIndex, OM::ToolStatus status)
	{
		auto tool = OM::GetTool(toolIndex);
		if (tool == nullptr)
		{
			return;
		}
		const bool statusChanged = (tool->status != status);
		tool->status = status;
		Colour c = /*(status == OM::ToolStatus::standby) ? colours->standbyBackColour : */
					(status == OM::ToolStatus::active) ? colours->activeBackColour
					: colours->buttonImageBackColour;
		if (tool->slot < MaxSlots)
		{
			toolButtons[tool->slot]->SetColours(colours->buttonTextColour, c);
		}
#if DISPLAY_X == 800
		if (statusChanged && currentUiPage == UiPage::ControlTools) RefreshControlToolsPage();
#endif
	}

	void SetToolExtruder(size_t toolIndex, uint8_t extruder)
	{
		OM::Tool *tool = OM::GetOrCreateTool(toolIndex);
		if (tool != nullptr)
		{
			tool->extruders.SetBit(extruder);
#if DISPLAY_X == 800
			if (controlExtrudeActiveToolCard != nullptr)
			{
				RefreshControlExtrudeTools();
			}
#endif
		}
	}

	void SetToolFan(size_t toolIndex, uint8_t fan)
	{
		OM::Tool *tool = OM::GetOrCreateTool(toolIndex);
		if (tool != nullptr)
		{
			tool->fans.SetBit(fan);
#if DISPLAY_X == 800
			if (currentUiPage == UiPage::StatusJobStatus)
			{
				RefreshJobStatusTilesByType(JobStatusTileType::FanPart);
				RefreshJobStatusTilesByType(JobStatusTileType::FanAux);
				RefreshJobStatusTilesByType(JobStatusTileType::FanCha);
			}
#endif
		}
	}

	// Clear the current tool-to-fan mapping before RRF supplies a fresh fan
	// array. This prevents removed/reassigned fans from remaining attached
	// to a tool in the local object-model mirror.
	void ClearToolFans(size_t toolIndex)
	{
		OM::Tool *tool = OM::GetOrCreateTool(toolIndex);
		if (tool != nullptr)
		{
			tool->fans.Clear();
#if DISPLAY_X == 800
			if (currentUiPage == UiPage::StatusJobStatus)
			{
				RefreshJobStatusTilesByType(JobStatusTileType::FanPart);
				RefreshJobStatusTilesByType(JobStatusTileType::FanAux);
				RefreshJobStatusTilesByType(JobStatusTileType::FanCha);
			}
#endif
		}
	}

	bool RemoveToolHeaters(const size_t toolIndex, const uint8_t firstIndexToDelete)
	{
		OM::Tool *tool = OM::GetOrCreateTool(toolIndex);
		if (tool == nullptr)
		{
			return false;
		}
		return tool->RemoveHeatersFrom(firstIndexToDelete) > 0;
	}

	void SetToolHeater(size_t toolIndex, uint8_t toolHeaterIndex, uint8_t heaterIndex)
	{
		OM::Tool *tool = OM::GetOrCreateTool(toolIndex);
		if (tool == nullptr)
		{
			return;
		}
		OM::ToolHeater *toolHeater = tool->GetOrCreateHeater(toolHeaterIndex);
		if (toolHeater == nullptr)
		{
			return;
		}
		toolHeater->heaterIndex = heaterIndex;
#if DISPLAY_X == 800
		if (controlExtrudeActiveToolCard != nullptr)
		{
			RefreshControlExtrudeTools();
		}
#endif
	}

	void SetToolOffset(size_t toolIndex, size_t axisIndex, float offset)
	{
		if (axisIndex < MaxTotalAxes)
		{
			OM::Tool *tool = OM::GetOrCreateTool(toolIndex);
			if (tool != nullptr)
			{
				tool->offsets[axisIndex] = offset;
			}
		}
	}

	// This handles the new path were spindles are assigned to tools
	void SetToolSpindle(int8_t toolIndex, int8_t spindleNumber)
	{
		// Old spindles[].tool is handled by SetSpindleTool
		OM::Tool *tool = OM::GetOrCreateTool(toolIndex);
		if (tool == nullptr)
		{
			return;
		}
		if (spindleNumber == -1)
		{
			tool->spindle = nullptr;
		}
		else
		{
			OM::Spindle* spindle = OM::GetSpindle(spindleNumber);
			if (spindle == nullptr)
			{
				return;
			}
			tool->spindle = spindle;
		}
	}

	void SetBabystepOffset(size_t index, float f)
	{
		if (index < MaxTotalAxes)
		{
			OM::Axis *axis = OM::GetOrCreateAxis(index);
			if (axis == nullptr)
			{
				return;
			}
			axis->babystep = f;
			// In first initialization we will see babystep before letter
			// so this won;t be true hence it is also set in UpdateGeometry
			if (axis->letter[0] == 'Z')
			{
				if (babystepOffsetField != nullptr)		// not created in the 800x480 build
				{
					babystepOffsetField->SetValue(f);
				}
			}
		}
	}

	void SetAxisLetter(size_t index, char l)
	{
		if (index < MaxTotalAxes)
		{
			OM::Axis *axis = OM::GetOrCreateAxis(index);
			if (axis != nullptr)
			{
				axis->letter[0] = l;
#if DISPLAY_X == 800
				const int oldXAxis = statusObjectXAxis, oldYAxis = statusObjectYAxis;
				if (l == 'X') statusObjectXAxis = static_cast<int>(index);
				else if (l == 'Y') statusObjectYAxis = static_cast<int>(index);
				if (oldXAxis != statusObjectXAxis || oldYAxis != statusObjectYAxis) statusObjectsDirty = true;
				RefreshControlMoveHoming();
#endif
			}
		}
	}

	void SetAxisVisible(size_t index, bool v)
	{
		if (index < MaxTotalAxes)
		{
			OM::Axis *axis = OM::GetOrCreateAxis(index);
			if (axis != nullptr)
			{
				axis->visible = v;
#if DISPLAY_X == 800
				RefreshControlMoveHoming();
#endif
			}
		}
	}

	void SetAxisWorkplaceOffset(size_t axisIndex, size_t workplaceIndex, float offset)
	{
		if (axisIndex < MaxTotalAxes && workplaceIndex < OM::Workplaces::MaxTotalWorkplaces)
		{
			OM::Axis *axis = OM::GetOrCreateAxis(axisIndex);
			if (axis != nullptr)
			{
				axis->workplaceOffsets[workplaceIndex] = offset;
			}
		}
	}

	void SetCurrentWorkplaceNumber(uint8_t workplaceNumber)
	{
		if (currentWorkplaceNumber == workplaceNumber || workplaceNumber >= OM::Workplaces::MaxTotalWorkplaces)
		{
			return;
		}
		currentWorkplaceNumber = workplaceNumber;
	}

	void SetBedOrChamberHeater(const uint8_t heaterIndex, const int8_t heaterNumber, bool bed)
	{
		if (bed)
		{
			auto bed = OM::GetOrCreateBed(heaterIndex);
			if (bed != nullptr)
			{
				bed->heater = heaterNumber;
			}
		}
		else
		{
			auto chamber = OM::GetOrCreateChamber(heaterIndex);
			if (chamber != nullptr)
			{
				chamber->heater = heaterNumber;
			}
		}
	}
}

// End
