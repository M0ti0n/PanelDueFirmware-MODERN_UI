/*
 * FileManager.cpp
 *
 * Created: 06/11/2015 10:52:13
 *  Author: David
 */

#include "FileManager.hpp"
#include <cctype>
#include "PanelDue.hpp"
#include <Hardware/SerialIo.hpp>
#include <Library/Misc.hpp>
#include <UI/UserInterface.hpp>
#include <UI/UserInterfaceConstants.hpp>
#include <General/Vector.hpp>
#include <General/String.h>

#undef min
#undef max

#if SAM4S
// Modern 800x480 build: independent caches sized to the 6-row UI.
// JOB: 96 entries = 16 pages. MACROS: 42 entries = 7 pages.
// The receive buffer is sized for the larger JOB list so a new listing can
// be received before it replaces the active cache.
constexpr size_t JobFileListSize = 2048;
constexpr size_t JobMaxFiles = 96;
constexpr size_t MacroFileListSize = 1024;
constexpr size_t MacroMaxFiles = 42;
constexpr size_t ReceiveFileListSize = 2048;
constexpr size_t ReceiveMaxFiles = 96;
#else
constexpr size_t JobFileListSize = 2048;
constexpr size_t JobMaxFiles = 96;
constexpr size_t MacroFileListSize = 1024;
constexpr size_t MacroMaxFiles = 42;
constexpr size_t ReceiveFileListSize = 2048;
constexpr size_t ReceiveMaxFiles = 96;
#endif

namespace FileManager
{
	typedef Vector<char, JobFileListSize> JobFileList;
	typedef Vector<const char* _ecv_array, JobMaxFiles> JobFileListIndex;
	typedef Vector<char, MacroFileListSize> MacroFileList;
	typedef Vector<const char* _ecv_array, MacroMaxFiles> MacroFileListIndex;
	typedef Vector<char, ReceiveFileListSize> ReceiveFileList;
	typedef Vector<const char* _ecv_array, ReceiveMaxFiles> ReceiveFileListIndex;

	const char * _ecv_array filesRoot = "0:/gcodes";
	const char * _ecv_array macrosRoot = "0:/macros";
	const uint32_t FileListRequestTimeout = 8000;				// file info request timeout in milliseconds

	static JobFileList jobFileList;
	static JobFileListIndex jobFileIndex;
	static MacroFileList macroFileList;
	static MacroFileListIndex macroFileIndex;
	static ReceiveFileList receiveFileList;
	static ReceiveFileListIndex receiveFileIndex;

	static bool receivingFileList = false;
	static int errorCode;
	static Path fileDirectoryName;
	static FileSet gcodeFilesList(filesRoot, NumDisplayedFiles, true);
	static FileSet macroFilesList(macrosRoot, NumDisplayedMacros, false);
	static FileSet * null displayedFileSet = nullptr;
	static uint8_t numVolumes = 1;								// how many SD card sockets we have (normally 1 or 2)

	// Return true if the second string is alphabetically greater then the first, case insensitive
	static inline bool StringGreaterThan(const char* a, const char* b)
	{
		return strcasecmp(a, b) > 0;
	}

	static size_t GetStoredFileCount(bool filesList)
	{
		return filesList ? jobFileIndex.Size() : macroFileIndex.Size();
	}

	static const char * _ecv_array GetStoredFile(bool filesList, size_t index)
	{
		return filesList ? jobFileIndex[index] : macroFileIndex[index];
	}

	template<class DestList, class DestIndex>
	static void CopyReceivedList(DestList& destList, DestIndex& destIndex)
	{
		destList.Clear();
		destIndex.Clear();
		for (size_t i = 0; i < receiveFileIndex.Size(); ++i)
		{
			const char * const name = receiveFileIndex[i];
			const size_t len = strlen(name) + 1;
			if (len + destList.Size() >= destList.Capacity() || destIndex.Size() >= destIndex.Capacity())
			{
				break;
			}
			destIndex.Add(destList.c_ptr() + destList.Size());
			destList.Add(name, len);
		}
	}

	FileSet::FileSet(const char * _ecv_array rootDir, unsigned int numDisp, bool pIsFilesList)
		: numDisplayed(numDisp), currentPath(), timer(FileListRequestTimeout, "", requestedPath.c_str()), listLoaded(false), scrollOffset(0), statusJobScrollOffset(0), controlMacroScrollOffset(0),
		  isFilesList(pIsFilesList), cardNumber(0)
	{
		requestedPath.copy(rootDir);
	}

	void FileSet::Display()
	{
		FileListUpdated();
		if (isFilesList)
		{
			UI::DisplayFilesPopup(cardNumber, numVolumes);
		}
		else
		{
			UI::DisplayMacrosPopup();
		}
		SetPending();							// refresh the list of files
	}

	void FileSet::Reload(const Path& dir, int errCode)
	{
		UI::FileListLoaded(isFilesList, errCode);			// do this first to show/hide the error message
		if (errCode == 0)
		{
			listLoaded = true;
			SetPath(dir.c_str());
		}
		else
		{
			listLoaded = false;
		}
		FileListUpdated();
		StatusJobPageUpdated();
		ControlMacrosPageUpdated();
		StopTimer();
	}

	// The macros list has just been updated. If this is for the root, update the short macro list, leaving out any subfolders.
	void FileSet::ReloadMacroShortList(int errorCode)
	{
		unsigned int buttonNum = 0;
		const MacroFileListIndex& index = macroFileIndex;
		unsigned int fileNum = (errorCode == 0) ? 0 : index.Size();
		bool again;
		do
		{
			// Find the next non-directory
			while (fileNum < index.Size() && index[fileNum][0] == '*')
			{
				++fileNum;
			}
			if (fileNum < index.Size())
			{
				again = UI::UpdateMacroShortList(buttonNum, index[fileNum]);
				++fileNum;
			}
			else
			{
				again = UI::UpdateMacroShortList(buttonNum, nullptr);
			}
			++buttonNum;
		} while (again);
	}

	// Refresh the list of files or macros in the Files popup window
	void FileSet::FileListUpdated()
	{
		if (listLoaded)
		{
			const size_t fileCount = GetStoredFileCount(isFilesList);

			// 2. Make sure the scroll position is still sensible
			if (scrollOffset < 0 || fileCount == 0)
			{
				scrollOffset = 0;
			}
			else if ((unsigned int)scrollOffset >= fileCount)
			{
				const unsigned int scrollAmount = UI::GetNumScrolledFiles(isFilesList);
				scrollOffset = ((fileCount - 1)/scrollAmount) * scrollAmount;
			}

			// 3. Display the scroll buttons if needed
			UI::EnableFileNavButtons(isFilesList, scrollOffset != 0, scrollOffset + numDisplayed < fileCount, IsInSubdir());

			if (isFilesList)
			{
				UI::FileListCardButtonUpdate(numVolumes);
			}

			// 4. Display the file list
			for (size_t i = 0; i < numDisplayed; ++i)
			{
				if (i + scrollOffset < fileCount)
				{
					const char *text = GetStoredFile(isFilesList, i + scrollOffset);
					UI::UpdateFileButton(isFilesList, i, (isFilesList) ? text : SkipDigitsAndUnderscore(text), text);
				}
				else
				{
					UI::UpdateFileButton(isFilesList, i, nullptr, nullptr);
				}
			}
			displayedFileSet = this;
		}
		else
		{
			if (isFilesList)
			{
				UI::DisplayFilesPopup(cardNumber, numVolumes);
			}
			else
			{
				UI::DisplayMacrosPopup();
			}
		}
	}

	void FileSet::Scroll(int amount)
	{
		scrollOffset += amount;
		FileListUpdated();
	}

	void FileSet::StatusJobPageUpdated()
	{
#if DISPLAY_X == 800
		static constexpr unsigned int JobRows = 6;
		if (!isFilesList)
		{
			return;
		}

		if (listLoaded)
		{
			const size_t fileCount = jobFileIndex.Size();
			if (statusJobScrollOffset < 0 || fileCount == 0)
			{
				statusJobScrollOffset = 0;
			}
			else if ((unsigned int)statusJobScrollOffset >= fileCount)
			{
				statusJobScrollOffset = ((fileCount - 1) / JobRows) * JobRows;
			}

			UI::EnableStatusJobNavButtons(statusJobScrollOffset != 0,
				statusJobScrollOffset + JobRows < fileCount, IsInSubdir());
			for (unsigned int i = 0; i < JobRows; ++i)
			{
				if (i + statusJobScrollOffset < fileCount)
				{
					const char * const entry = jobFileIndex[i + statusJobScrollOffset];
					UI::UpdateStatusJobFileButton(i, entry, entry);
				}
				else
				{
					UI::UpdateStatusJobFileButton(i, nullptr, nullptr);
				}
			}
		}
		else
		{
			UI::EnableStatusJobNavButtons(false, false, false);
			for (unsigned int i = 0; i < JobRows; ++i)
			{
				UI::UpdateStatusJobFileButton(i, nullptr, nullptr);
			}
		}
#endif
	}

	void FileSet::DisplayStatusJobPage()
	{
		StatusJobPageUpdated();
		SetPending();
	}

	void FileSet::ScrollStatusJobPage(int amount)
	{
		statusJobScrollOffset += amount;
		StatusJobPageUpdated();
	}

	void FileSet::RequestStatusJobSubdir(const char * _ecv_array dir)
	{
		statusJobScrollOffset = 0;
		listLoaded = false;
		StatusJobPageUpdated();

		requestedPath.copy(currentPath.c_str());
		if (requestedPath.strlen() == 0 || requestedPath[requestedPath.strlen() - 1] != '/')
		{
			requestedPath.cat('/');
		}
		requestedPath.cat(dir);
		SetPending();
	}

	void FileSet::RequestStatusJobParentDir()
	{
		statusJobScrollOffset = 0;
		listLoaded = false;
		StatusJobPageUpdated();

		size_t end = currentPath.strlen();
		if (end != 0 && currentPath[end - 1] == '/')
		{
			--end;
		}
		while (end != 0)
		{
			--end;
			if (currentPath[end] == '/')
			{
				break;
			}
		}
		requestedPath.Clear();
		for (size_t i = 0; i < end; ++i)
		{
			requestedPath.cat(currentPath[i]);
		}
		SetPending();
	}

	// Modern CONTROL > MACROS page. It shares the macro FileSet with the macro short list
	// but keeps its own scroll offset, mirroring the STATUS > JOB page for gcode files.
	void FileSet::ControlMacrosPageUpdated()
	{
#if DISPLAY_X == 800
		static constexpr unsigned int MacroRows = 6;
		if (isFilesList)
		{
			return;
		}

		if (listLoaded)
		{
			const size_t fileCount = macroFileIndex.Size();
			if (controlMacroScrollOffset < 0 || fileCount == 0)
			{
				controlMacroScrollOffset = 0;
			}
			else if ((unsigned int)controlMacroScrollOffset >= fileCount)
			{
				controlMacroScrollOffset = ((fileCount - 1) / MacroRows) * MacroRows;
			}

			UI::EnableControlMacroNavButtons(controlMacroScrollOffset != 0,
				controlMacroScrollOffset + MacroRows < fileCount, IsInSubdir());
			for (unsigned int i = 0; i < MacroRows; ++i)
			{
				if (i + controlMacroScrollOffset < fileCount)
				{
					const char * const entry = macroFileIndex[i + controlMacroScrollOffset];
					UI::UpdateControlMacroFileButton(i, entry, entry);
				}
				else
				{
					UI::UpdateControlMacroFileButton(i, nullptr, nullptr);
				}
			}
		}
		else
		{
			UI::EnableControlMacroNavButtons(false, false, false);
			for (unsigned int i = 0; i < MacroRows; ++i)
			{
				UI::UpdateControlMacroFileButton(i, nullptr, nullptr);
			}
		}
#endif
	}

	void FileSet::DisplayControlMacrosPage()
	{
		ControlMacrosPageUpdated();
		SetPending();
	}

	void FileSet::ScrollControlMacrosPage(int amount)
	{
		controlMacroScrollOffset += amount;
		ControlMacrosPageUpdated();
	}

	void FileSet::RequestControlMacrosSubdir(const char * _ecv_array dir)
	{
		controlMacroScrollOffset = 0;
		listLoaded = false;
		ControlMacrosPageUpdated();

		requestedPath.copy(currentPath.c_str());
		if (requestedPath.strlen() == 0 || requestedPath[requestedPath.strlen() - 1] != '/')
		{
			requestedPath.cat('/');
		}
		requestedPath.cat(dir);
		SetPending();
	}

	void FileSet::RequestControlMacrosParentDir()
	{
		controlMacroScrollOffset = 0;
		listLoaded = false;
		ControlMacrosPageUpdated();

		size_t end = currentPath.strlen();
		if (end != 0 && currentPath[end - 1] == '/')
		{
			--end;
		}
		while (end != 0)
		{
			--end;
			if (currentPath[end] == '/')
			{
				break;
			}
		}
		requestedPath.Clear();
		for (size_t i = 0; i < end; ++i)
		{
			requestedPath.cat(currentPath[i]);
		}
		SetPending();
	}

	void FileSet::SetPath(const char * _ecv_array pPath)
	{
		currentPath.copy(pPath);
	}

	// Return true if the path has more than one directory component on card 0, or at least one directory component on other cards
	bool FileSet::IsInSubdir() const
	{
		// Find the start of the first component of the path
		size_t start = (currentPath.strlen() >= 2 && isdigit(currentPath[0]) && currentPath[1] == ':') ? 2 : 0;
		if (currentPath[start] == '/')
		{
			++start;
		}
		size_t end = currentPath.strlen();
		if (end > start && currentPath[end - 1] == '/')
		{
			--end;			// remove trailing '/' if there is one
		}
		// If we are on card 0, then /gcodes or /macros is the effective root, so skip one path component
		if (cardNumber == 0)
		{
			while (end != 0 && currentPath[end] != '/')
			{
				--end;
			}
		}
		return (end > start);
	}

	// Request the parent path
	void FileSet::RequestParentDir()
	{
		listLoaded = false;
		FileListUpdated();									// this hides the file list until we receive a new one

		size_t end = currentPath.strlen();
		// Skip any trailing '/'
		if (end != 0 && currentPath[end - 1] == '/')
		{
			--end;
		}
		// Find the last '/'
		while (end != 0)
		{
			--end;
			if (currentPath[end] == '/')
			{
				break;
			}
		}
		requestedPath.Clear();
		for (size_t i = 0; i < end; ++i)
		{
			requestedPath.cat(currentPath[i]);
		}
		SetPending();
	}

	// Build a subdirectory of the current path
	void FileSet::RequestSubdir(const char * _ecv_array dir)
	{
		listLoaded = false;
		FileListUpdated();									// this hides the file list until we receive a new one

		requestedPath.copy(currentPath.c_str());
		if (requestedPath.strlen() == 0 || (requestedPath[requestedPath.strlen() - 1] != '/'))
		{
			requestedPath.cat('/');
		}
		requestedPath.cat(dir);
		SetPending();
	}

	// Use the timer to send a command and repeat it if no response is received
	void FileSet::SetPending()
	{
		timer.SetCommand(GetFirmwareFeatures().IsBitSet(noM20M36) ? "M408 S20 P" : "M20 S2 P");
		timer.SetArgument(CondStripDrive(requestedPath.c_str()), GetFirmwareFeatures().IsBitSet(quoteFilenames));
		timer.SetPending();
	}

	// Select the next SD card
	bool FileSet::NextCard()
	{
		if (isFilesList && numVolumes > 1)
		{
			unsigned int cn = cardNumber + 1;
			if (cn >= numVolumes)
			{
				cn = 0;
			}
			return SelectCard(cn);
		}
		return false;
	}

	// Select a particular SD card
	bool FileSet::SelectCard(unsigned int cardNum)
	{
		if (isFilesList && cardNum < numVolumes)
		{
			cardNumber = cardNum;
			UI::DisplayFilesPopup(cardNumber, numVolumes);
			listLoaded = false;
			FileListUpdated();								// this hides the file list until we receive a new one

			if (cardNumber == 0)
			{
				SetupRootPath();
			}
			else
			{
				// Send a command to mount the removable card. RepRapFirmware will ignore it if the card is already mounted and there are any files open on it.
				SerialIo::Sendf("M21 P%d\n", cardNumber);
				requestedPath.printf("%u:", (unsigned int)cardNumber);
			}
			SetPending();
			return true;
		}
		return false;
	}

	void FileSet::SetupRootPath()
	{
		requestedPath.copy((cardNumber == 0 && GetFirmwareFeatures().IsBitSet(noGcodesFolder)) ? "0:/" : filesRoot);
	}

	// This is called on the gcode files list when the firmware features are changed from the previous values
	void FileSet::FirmwareFeaturesChanged()
	{
		SetupRootPath();
	}

	// This is called when a new JSON response is received
	void BeginNewMessage()
	{
		fileDirectoryName.Clear();
		errorCode = 0;
		receivingFileList = false;
	}

	// This is called at the end of a JSON response
	void EndReceivedMessage()
	{
		if (receivingFileList)
		{
			// We received a new file list, which may be for the files or the macro list. Find out which.
			size_t i;
			bool card0;
			if (fileDirectoryName.strlen() >= 2 && isdigit(fileDirectoryName[0]) && fileDirectoryName[1] == ':')
			{
				i = 2;
				card0 = (fileDirectoryName[0] == '0');
			}
			else
			{
				i = 0;
				card0 = true;
			}
			if (fileDirectoryName[i] == '/')
			{
				++i;
			}
			String<10> temp;
			while (i < fileDirectoryName.strlen() && fileDirectoryName[i] != '/')
			{
				temp.cat(fileDirectoryName[i++]);
			}

			receiveFileIndex.Sort([](auto a, auto b) -> bool { return StringGreaterThan(a, b); });

			if (card0 && temp.EqualsIgnoreCase("macros"))
			{
				if (errorCode == 0)
				{
					CopyReceivedList(macroFileList, macroFileIndex);
				}
				macroFilesList.Reload(fileDirectoryName, errorCode);
				if (i + 1 >= fileDirectoryName.strlen())				// if in root of /macros
				{
					macroFilesList.ReloadMacroShortList(errorCode);
				}
			}
			else if (!UI::IsDisplayingFileInfo())
			{
				if (errorCode == 0)
				{
					CopyReceivedList(jobFileList, jobFileIndex);
				}
				gcodeFilesList.Reload(fileDirectoryName, errorCode);
			}
			receivingFileList = false;
		}
	}

	// This is called when we start receiving a list of files
	void BeginReceivingFiles()
	{
		receiveFileList.Clear();
		receiveFileIndex.Clear();
		receivingFileList = true;
	}

	// This is called for each filename received
	void ReceiveFile(const char * _ecv_array data)
	{
		if (receivingFileList)
		{
			const size_t len = strlen(data) + 1;		// we are going to copy the null terminator as well
			if (len + receiveFileList.Size() < receiveFileList.Capacity() && receiveFileIndex.Size() < receiveFileIndex.Capacity())
			{
				receiveFileIndex.Add(receiveFileList.c_ptr() + receiveFileList.Size());
				receiveFileList.Add(data, len);
			}
		}
	}

	// This is called when we receive the directory name
	void ReceiveDirectoryName(const char * _ecv_array data)
	{
		fileDirectoryName.copy(data);
	}

	// This is called when we receive an error code
	void ReceiveErrorCode(int err)
	{
		if (receivingFileList)
		{
			// We have received a file list, so this error code relates to it
			errorCode = err;
		}
	}

	void DisplayFilesList()
	{
		gcodeFilesList.Display();
	}

	void DisplayFilesPage()
	{
		gcodeFilesList.DisplayStatusJobPage();
	}

	void ScrollFilesPage(int amount)
	{
		gcodeFilesList.ScrollStatusJobPage(amount);
	}

	void RequestFilesPageSubdir(const char * _ecv_array dir)
	{
		gcodeFilesList.RequestStatusJobSubdir(dir);
	}

	void RequestFilesPageParentDir()
	{
		gcodeFilesList.RequestStatusJobParentDir();
	}

	void DisplayMacrosList()
	{
		macroFilesList.Display();
	}

	void DisplayControlMacrosPage()
	{
		macroFilesList.DisplayControlMacrosPage();
	}

	void ScrollControlMacrosPage(int amount)
	{
		macroFilesList.ScrollControlMacrosPage(amount);
	}

	void RequestControlMacrosSubdir(const char * _ecv_array dir)
	{
		macroFilesList.RequestControlMacrosSubdir(dir);
	}

	void RequestControlMacrosParentDir()
	{
		macroFilesList.RequestControlMacrosParentDir();
	}

	void ScrollFiles(int amount)
	{
		gcodeFilesList.Scroll(amount);
	}

	void ScrollMacros(int amount)
	{
		macroFilesList.Scroll(amount);
	}

	void RequestFilesSubdir(const char * _ecv_array dir)
	{
		gcodeFilesList.RequestSubdir(dir);
	}

	void RequestMacrosSubdir(const char * _ecv_array dir)
	{
		macroFilesList.RequestSubdir(dir);
	}

	void RequestFilesParentDir()
	{
		gcodeFilesList.RequestParentDir();
	}

	void RequestMacrosParentDir()
	{
		macroFilesList.RequestParentDir();
	}

	const char * _ecv_array GetFilesDir()
	{
		return gcodeFilesList.GetPath();
	}

	const char * _ecv_array GetMacrosDir()
	{
		return macroFilesList.GetPath();
	}

	const char * _ecv_array GetMacrosRootDir()
	{
		return macrosRoot;
	}

	void RefreshFilesList()
	{
		gcodeFilesList.SetPending();
	}

	void RefreshMacrosList()
	{
		macroFilesList.SetPending();
	}

	// This is called from the main loop to check for timer events
	bool ProcessTimers()
	{
		bool sent = macroFilesList.ProcessTimer();
		if (!sent)
		{
			sent = gcodeFilesList.ProcessTimer();
		}
		return sent;
	}

	bool NextCard()
	{
		return gcodeFilesList.NextCard();
	}

	bool SelectCard(unsigned int cardNum)
	{
		return gcodeFilesList.SelectCard(cardNum);
	}

	void SetNumVolumes(unsigned int n)
	{
		if (n > 0 && n <= 10)
		{
			numVolumes = n;
		}
	}

	// This is called when the host tells us its firmware type, and it is not the type we were previously assuming
	void FirmwareFeaturesChanged()
	{
		gcodeFilesList.FirmwareFeaturesChanged();
	}
}		// end namespace

// End
