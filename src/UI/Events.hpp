/*
 * Events.hpp
 *
 *  Created on: 6 Jan 2017
 *      Author: David
 */

#ifndef SRC_UI_EVENTS_HPP_
#define SRC_UI_EVENTS_HPP_

// Event numbers, used to say what we need to do when a field is touched
// *** MUST leave value 0 free to mean "no event"
enum Event : uint8_t
{
	evNull = 0,                        // value must match nullEvent declared in Display.hpp

	evDefaultRoot, evScreensaverRoot,

	// Page selection
	evTabControl, evTabStatus, evTabSystem, evTabMsg, evTabSetup,

	// Heater control
	evSelectHead, evSelectBed, evSelectChamber,
	evAdjustToolActiveTemp, evAdjustToolStandbyTemp,
	evAdjustBedActiveTemp, evAdjustBedStandbyTemp,
	evAdjustChamberActiveTemp, evAdjustChamberStandbyTemp,

	// Spindle control
	evAdjustActiveRPM,

	// Control functions
	evMovePopup, evExtrudePopup, evFan, evListMacros,
	evMoveAxis,
	evMoveSelectAxis,
	evExtrudeAmount, evExtrudeRate, evExtrude, evRetract,
	evHomeAxis,

	// Print functions
	evExtrusionFactor,
	evAdjustFan,
	evAdjustInt,
	evSetInt,
	evListFiles,

	evFile, evMacro, evMacroControlPage,
	evPrintFile,
	evSendCommand,
	evFactoryReset,
	evAdjustSpeed,

	evScrollFiles, evScrollMacros, evFilesUp, evMacrosUp, evChangeCard,

	evKeyboard,

	// Setup functions
	evCalTouch, evSetBaudRate, evInvertX, evInvertY, evAdjustBaudRate, evSetVolume, evAdjustVolume, evSetInfoTimeout, evAdjustInfoTimeout, evReset,

	evYes,
	evCancel,
	evDeleteFile,
	evSimulateFile,
	evPausePrint,
	evResumePrint,
	evReprint, evResimulate,
	evBabyStepPopup, evBabyStepMinus, evBabyStepPlus,

	evKey, evShift, evBackspace, evSendKeyboardCommand, evUp, evDown,

	evAdjustColours, evSetColours,
	evBrighter, evDimmer,
	evSetDimmingType,
	evSetScreensaverTimeout, evAdjustScreensaverTimeout,
	evSetBabystepAmount, evAdjustBabystepAmount,
	evSetFeedrate, evAdjustFeedrate,
	evSetHeaterCombineType,
	evSetLogLevel,

	evEmergencyStop,

	evJogZ,
	evCloseAlert, evOkAlert, evChoiceAlert, evEditAlert,

	// Subpage events for the vertical tabs
	// CONTROL subpages
	evControlTools,
	evControlMovement,
	evControlExtrusion,
	evControlMacros,
	evControlToolsPageUp,
	evControlToolsPageDown,
	evControlToolsPower,
	evControlToolsActiveTemp,
	evControlToolsStandbyTemp,
	evControlToolsHeaderTap,
	evControlToolChangeConfirm,
	evControlToolChangeCancel,
	evControlHeaterOffConfirm,
	evControlHeaterOffCancel,
	evControlMoveStep,
	evControlMoveJog,
	evControlMoveHome,
	evControlMoveBedComp,
	evModernAlertClose,
	evModernInfoClose,
	evControlExtrudeSpeed,
	evControlExtrudeDistance,
	evControlExtrudeAction,
	evControlExtrudePageUp,
	evControlExtrudePageDown,

	// STATUS subpages
	evStatusJobStatus,
	evStatusTune,
	evStatusJob,
	evStatusObjects,
	evStatusObject1,
	evStatusObject2,
	evStatusObject3,
	evStatusObject4,
	evStatusObject5,
	evStatusObject6,
	evStatusObjectPageUp,
	evStatusObjectPageDown,
	evStatusObjectSelect,
	evStatusObjectNumber,
	evStatusObjectMarker,
	evStatusObjectCancelConfirm,
	evStatusObjectCancelClose,

	// STATUS > TUNE controls
	evTuneSpeed,
	evTuneGeneralFan,
	evTuneToolFan,
	evTuneToolFlow,
	evTunePressureAdvance,
	evTunePageUp,
	evTunePageDown,
	evTuneZMinus,
	evTuneZPlus,
	evTunePopupAdjustPercent,
	evTunePopupAdjustPa,
	evTunePopupConfirm,
	evTunePopupCancel,

	// CONTROL > MACROS controls
	evControlMacroFile,
	evControlMacroPageUp,
	evControlMacroPageDown,
	evControlMacroRunConfirm,
	evControlMacroRunCancel,

	// STATUS > JOB controls
	evStatusJobFile,
	evStatusJobPageUp,
	evStatusJobPageDown,
	evStatusJobPrintConfirm,
	evStatusJobPrintCancel,
	evStatusJobDeleteOpen,
	evStatusJobDeleteConfirm,
	evStatusJobDeleteCancel,

	// STATUS > JOB STATUS controls
	evStatusJobStatusPauseResume,
	evStatusJobStatusAbort,
	evStatusJobStatusConfirm,
	evStatusJobStatusCancel,

	// SYSTEM subpages
	evSystemConsole,
	evSystemSettings,

	// SYSTEM > SETTINGS modern controls
	evSettingsVolumeOpen, evSettingsVolumeSelect, evSettingsVolumeConfirm,
	evSettingsBrightnessOpen, evSettingsBrightnessSelect, evSettingsBrightnessConfirm,
	evSettingsInfoTimeoutOpen, evSettingsInfoTimeoutSelect, evSettingsInfoTimeoutConfirm,
	evSettingsAccentOpen, evSettingsAccentSelect, evSettingsAccentConfirm,
	evSettingsAlwaysDimToggle,
	evSettingsBaudOpen, evSettingsBaudSelect, evSettingsBaudConfirm,
	evSettingsTouchOpen, evSettingsTouchConfirm,
	evSettingsHeaterCombineOpen, evSettingsHeaterCombineConfirm,
	evSettingsFactoryResetOpen, evSettingsFactoryResetConfirm,
	evSettingsPopupCancel,

	// Extrusion length numeric trigger
	evAdjustExtrudeLength,

	// Numeric pad events (for full numeric input popup)
	evNumericKey,     // integer iParam = ASCII code of digit or '.'; used while numeric pad is active
	evNumericBack,    // backspace key on numeric pad
	evNumericOk,      // confirm numeric pad value
	evNumericCancel,  // cancel numeric pad

	// Shared reusable standard popup events. Appended here so existing event values stay stable.
	evStandardPopupChoice,
	evStandardPopupConfirm,
	evStandardPopupCancel,

	// Filasnake (SYSTEM > SETTINGS easter egg). Appended so existing event values stay stable.
	evFilasnakeOpen,
	evFilasnakeDir,		// iParam = direction 0 up, 1 right, 2 down, 3 left
	evFilasnakeGo,

};

#endif /* SRC_UI_EVENTS_HPP_ */
