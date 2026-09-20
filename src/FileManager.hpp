/*
 * FileManager.h
 *
 * Created: 06/11/2015 10:52:38
 *  Author: David
 */ 

#ifndef FILEMANAGER_H_
#define FILEMANAGER_H_

#include "Configuration.hpp"
#include "RequestTimer.hpp"
#include "FirmwareFeatures.hpp"
#include <General/String.h>

namespace FileManager
{
	const size_t maxPathLength = 100;
	typedef String<maxPathLength> Path;

	class FileSet
	{
	private:
		const unsigned numDisplayed;
		Path requestedPath;
		Path currentPath;
		RequestTimer timer;
		int whichList;
		int scrollOffset;
		int statusJobScrollOffset;
		int controlMacroScrollOffset;
		bool IsInSubdir() const;
		const bool isFilesList;			// true for a file list, false for a macro list
		uint8_t cardNumber;

	public:
		FileSet(const char * _ecv_array rootDir, unsigned numDisp, bool pIsFilesList);
		void Display();
		void Reload(int whichList, const Path& dir, int errCode);
		void ReloadMacroShortList(int errorCode);
		void FileListUpdated();
		void Scroll(int amount);
		void SetIndex(int index) { whichList = index; }
		int GetIndex() const { return whichList; }
		void SetPath(const char * _ecv_array pPath);
		const char * _ecv_array GetPath() { return currentPath.c_str(); }
		void RequestParentDir()
			pre(IsInSubdir());
		void RequestSubdir(const char * _ecv_array dir);
		void SetPending();
		void StopTimer() { timer.Stop(); }
		bool ProcessTimer() { return timer.Process(); }
		bool NextCard();
		bool SelectCard(unsigned int cardNum);
		void FirmwareFeaturesChanged();
		void DisplayStatusJobPage();
		void ScrollStatusJobPage(int amount);
		void RequestStatusJobSubdir(const char * _ecv_array dir);
		void RequestStatusJobParentDir();
		void DisplayControlMacrosPage();
		void ScrollControlMacrosPage(int amount);
		void RequestControlMacrosSubdir(const char * _ecv_array dir);
		void RequestControlMacrosParentDir();

	private:
		void SetupRootPath();
		void StatusJobPageUpdated();
		void ControlMacrosPageUpdated();
	};

	void BeginNewMessage();
	void EndReceivedMessage();
	void BeginReceivingFiles();
	void ReceiveFile(const char * _ecv_array data);
	void ReceiveDirectoryName(const char * _ecv_array data);
	void ReceiveErrorCode(int err);

	void DisplayFilesList();
	void DisplayFilesPage();				// refresh the embedded STATUS > JOB list without opening the legacy popup
	void ScrollFilesPage(int amount);
	void RequestFilesPageSubdir(const char * _ecv_array dir);
	void RequestFilesPageParentDir();
	void DisplayMacrosList();
	void DisplayControlMacrosPage();		// refresh the embedded CONTROL > MACROS list without opening the legacy popup
	void ScrollControlMacrosPage(int amount);
	void RequestControlMacrosSubdir(const char * _ecv_array dir);
	void RequestControlMacrosParentDir();
	void ScrollFiles(int amount);
	void ScrollMacros(int amount);

	void RequestFilesSubdir(const char * _ecv_array dir);
	void RequestMacrosSubdir(const char * _ecv_array dir);
	void RequestFilesParentDir();
	void RequestMacrosParentDir();
	const char * _ecv_array GetFilesDir();
	const char * _ecv_array GetMacrosDir();
	const char * _ecv_array GetMacrosRootDir();

	void RefreshFilesList();
	void RefreshMacrosList();
	bool ProcessTimers();
	bool NextCard();
	bool SelectCard(unsigned int cardNum);
	void SetNumVolumes(size_t n);
	void FirmwareFeaturesChanged();
}

#endif /* FILEMANAGER_H_ */
