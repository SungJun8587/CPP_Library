
//***************************************************************************
// HardwareInfo.h: interface for the Hardware Information Class.
//
//***************************************************************************

#ifndef __HARDWAREINFO_H__
#define __HARDWAREINFO_H__

#ifndef _INC_MMSYSTEM
#include <mmsystem.h>
#endif

#ifndef _INC_MMREG
#include <mmreg.h>
#endif

#ifndef __BASEARRAYLIST_H__
#include <BaseArrayList.h>
#endif

#ifndef __WMI_H__
#include <Wmi.h>
#endif

void ChangeDataFormat(const __int64& nData, TCHAR *ptszFormat);

//***************************************************************************
//
typedef struct _HWINFO_BIOS
{
public:
	_HWINFO_BIOS() {
		m_tszManufacturer[0] = '\0';
		m_tszSmVersion[0] = '\0';
		m_tszVersion[0] = '\0';
		m_tszIdentificationCode[0] = '\0';
		m_tszSerialNumber[0] = '\0';
		m_tszReleaseDate[0] = '\0';
	}

	TCHAR	m_tszManufacturer[BIOS_MANUFACTURER_STRLEN];
	TCHAR	m_tszSmVersion[BIOS_SMVERSION_STRLEN];
	TCHAR	m_tszVersion[BIOS_VERSION_STRLEN];
	TCHAR	m_tszIdentificationCode[BIOS_IDENTIFICATIONCODE_STRLEN];
	TCHAR	m_tszSerialNumber[BIOS_SERIALNUMBER_STRLEN];
	TCHAR	m_tszReleaseDate[BIOS_RELEASEDATE_STRLEN];

} HWINFO_BIOS, *PHWINFO_BIOS;

//***************************************************************************
//
typedef struct _HWINFO_MAINBOARD
{
public:
	_HWINFO_MAINBOARD() {
		m_tszProduct[0] = '\0';
		m_tszSerialNumber[0] = '\0';
		m_tszManufacturer[0] = '\0';
		m_tszDescription[0] = '\0';
	}

	TCHAR	m_tszProduct[MAINBOARD_PRODUCT_STRLEN];
	TCHAR	m_tszSerialNumber[MAINBOARD_SERIALNUMBER_STRLEN];
	TCHAR	m_tszManufacturer[MAINBOARD_MANUFACTURER_STRLEN];
	TCHAR	m_tszDescription[MAINBOARD_DESCRIPTION_STRLEN];

} HWINFO_MAINBOARD, *PHWINFO_MAINBOARD;

//***************************************************************************
//
typedef struct _HWINFO_RAM
{
public:
	_HWINFO_RAM() {
		m_nCapacity = 0;
		m_dwFormFactor = 0;
		m_dwMemoryType = 0;
		m_dwSpeed = 0;

		m_tszBankLabel[0] = '\0';
		m_tszName[0] = '\0';
		m_tszDeviceLocator[0] = '\0';
		m_tszFormFactorDesc[0] = '\0';
		m_tszMemoryTypeDesc[0] = '\0';
	}

	__int64		m_nCapacity;
	DWORD		m_dwFormFactor;
	DWORD		m_dwMemoryType;
	DWORD		m_dwSpeed;
	TCHAR		m_tszBankLabel[RAM_BANKLABEL_STRLEN];
	TCHAR		m_tszName[RAM_NAME_STRLEN];
	TCHAR		m_tszDeviceLocator[RAM_DEVICELOCATOR_STRLEN];
	TCHAR		m_tszFormFactorDesc[RAM_FORMFACTORDESC_STRLEN];
	TCHAR		m_tszMemoryTypeDesc[RAM_MEMORYTYPEDESC_STRLEN];

} HWINFO_RAM, *PHWINFO_RAM;

//***************************************************************************
//
typedef struct _HWINFO_MEMORY
{
public:
	_HWINFO_MEMORY() {
		m_dwRamCount = 0;
		m_nTotalMemSize = 0;
		m_nPhysicalMemSize = 0;
		m_nTotalVirtualMemSize = 0;
		m_nFreeVirtualMemSize = 0;
		m_nTotalPageFileSize = 0;
		m_nFreePageFileSize = 0;
	}

	DWORD		m_dwRamCount;
	__int64		m_nTotalMemSize;
	__int64		m_nPhysicalMemSize;
	__int64		m_nTotalVirtualMemSize;
	__int64		m_nFreeVirtualMemSize;
	__int64		m_nTotalPageFileSize;
	__int64		m_nFreePageFileSize;

} HWINFO_MEMORY, *PHWINFO_MEMORY;

//***************************************************************************
//
typedef struct _HWINFO_HDDISK
{
public:
	_HWINFO_HDDISK() {
		m_nTotalSize = 0;

		m_tszModel[0] = '\0';
		m_tszName[0] = '\0';
		m_tszManufacturer[0] = '\0';
		m_tszDescription[0] = '\0';
	}

	__int64		m_nTotalSize;

	TCHAR	m_tszModel[HDDISK_MODEL_STRLEN];
	TCHAR	m_tszName[HDDISK_NAME_STRLEN];
	TCHAR	m_tszManufacturer[HDDISK_MANUFACTURER_STRLEN];
	TCHAR	m_tszDescription[HDDISK_DESCRIPTION_STRLEN];

} HWINFO_HDDISK, *PHWINFO_HDDISK;

//***************************************************************************
//
typedef struct _HWINFO_DRIVE
{
public:
	_HWINFO_DRIVE() {
		m_nTotalSpace = 0;
		m_nFreeSpace = 0;

		m_tszName[0] = '\0';
		m_tszFileSystem[0] = '\0';
	}

	__int64		m_nTotalSpace;
	__int64		m_nFreeSpace;

	TCHAR	m_tszName[DRIVE_NAME_STRLEN];
	TCHAR	m_tszFileSystem[DRIVE_FILESYSTEM_STRLEN];

} HWINFO_DRIVE, *PHWINFO_DRIVE;

//***************************************************************************
//
typedef struct _HWINFO_DRIVES
{
public:
	_HWINFO_DRIVES() {
		m_dwDriveCount = 0;
		m_nTotalSpace = 0;
		m_nFreeSpace = 0;
	}

	DWORD		m_dwDriveCount;
	__int64		m_nTotalSpace;
	__int64		m_nFreeSpace;

} HWINFO_DRIVES, *PHWINFO_DRIVES;

//***************************************************************************
//
typedef struct _HWINFO_SOUNDCARD
{
public:
	_HWINFO_SOUNDCARD() {
		m_bHasVolCtrl = false;
		m_bHasSeparateLRVolCtrl = false;

		m_tszProductName[0] = '\0';
		m_tszCompanyName[0] = '\0';
	}

	BOOL	m_bHasVolCtrl;
	BOOL	m_bHasSeparateLRVolCtrl;

	TCHAR   m_tszProductName[SOUNDCARD_PRODUCTNAME_STRLEN];
	TCHAR	m_tszCompanyName[SOUNDCARD_COMPANYNAME_STRLEN];

} HWINFO_SOUNDCARD, *PHWINFO_SOUNDCARD;

//***************************************************************************
//
typedef struct _HWINFO_VIDEOCARD
{
public:
	_HWINFO_VIDEOCARD() {
		m_lMemorySize = 0;

		m_tszDescription[0] = '\0';
		m_tszAdapterString[0] = '\0';
		m_tszChipType[0] = '\0';
		m_tszDacType[0] = '\0';
		m_tszDisplayDrivers[0] = '\0';
	}

	long	m_lMemorySize;

	TCHAR	m_tszDescription[VIDEOCARD_DESCRIPTION_STRLEN];
	TCHAR	m_tszAdapterString[VIDEOCARD_ADAPTERSTRING_STRLEN];
	TCHAR   m_tszChipType[VIDEOCARD_CHIPTYPE_STRLEN];
	TCHAR	m_tszDacType[VIDEOCARD_DACTYPE_STRLEN];
	TCHAR   m_tszDisplayDrivers[VIDEOCARD_DISPLAYDRIVERS_STRLEN];

} HWINFO_VIDEOCARD, *PHWINFO_VIDEOCARD;

//***************************************************************************
//
typedef struct _HWINFO_NETWORKCARD
{
public:
	_HWINFO_NETWORKCARD() {
		m_tszDescription[0] = '\0';
	}

	TCHAR	m_tszDescription[NETWORKCARD_DESCRIPTION_STRLEN];

} HWINFO_NETWORKCARD, *PHWINFO_NETWORKCARD;

//***************************************************************************
//
typedef struct _HWINFO_CDROM
{
public:
	_HWINFO_CDROM() {
		m_tszName[0] = '\0';
		m_tszManufacturer[0] = '\0';
		m_tszDescription[0] = '\0';
	}

	TCHAR	m_tszName[CDROM_NAME_STRLEN];
	TCHAR	m_tszManufacturer[CDROM_MANUFACTURER_STRLEN];
	TCHAR	m_tszDescription[CDROM_DESCRIPTION_STRLEN];

} HWINFO_CDROM, *PHWINFO_CDROM;

//***************************************************************************
//
typedef struct _HWINFO_KEYBOARD
{
public:
	_HWINFO_KEYBOARD() {
		m_tszDescription[0] = '\0';
		m_tszType[0] = '\0';
	}

	TCHAR	m_tszDescription[KEYBOARD_DESCRIPTION_STRLEN];
	TCHAR	m_tszType[KEYBOARD_TYPE_STRLEN];

} HWINFO_KEYBOARD, *PHWINFO_KEYBOARD;

//***************************************************************************
//
typedef struct _HWINFO_MOUSE
{
public:
	_HWINFO_MOUSE() {
		m_tszName[0] = '\0';
		m_tszManufacturer[0] = '\0';
		m_tszDescription[0] = '\0';
	}

	TCHAR	m_tszName[MOUSE_NAME_STRLEN];
	TCHAR	m_tszManufacturer[MOUSE_MANUFACTURER_STRLEN];
	TCHAR	m_tszDescription[MOUSE_DESCRIPTION_STRLEN];

} HWINFO_MOUSE, *PHWINFO_MOUSE;

//***************************************************************************
//
typedef struct _HWINFO_MONITOR
{
public:
	_HWINFO_MONITOR() {
		m_tszManufacturer[0] = '\0';
		m_tszDescription[0] = '\0';
	}

	TCHAR	m_tszManufacturer[MONITOR_MANUFACTURER_STRLEN];
	TCHAR	m_tszDescription[MONITOR_DESCRIPTION_STRLEN];

} HWINFO_MONITOR, *PHWINFO_MONITOR;

//***************************************************************************
//
class CBiosInfo
{
public:
	CBiosInfo();
	~CBiosInfo();

	BOOL GetInformation(CWmi &Wmi);

	const TCHAR* GetManufacturer() const {
		return m_Bios.m_tszManufacturer;
	}
	const TCHAR* GetSmVersion() const {
		return m_Bios.m_tszSmVersion;
	}
	const TCHAR* GetVersion() const {
		return m_Bios.m_tszVersion;
	}
	const TCHAR* GetIdentificationCode() const {
		return m_Bios.m_tszIdentificationCode;
	}
	const TCHAR* GetSerialNumber() const {
		return m_Bios.m_tszSerialNumber;
	}
	const TCHAR* GetReleaseDate() const {
		return m_Bios.m_tszReleaseDate;
	}

private:
	HWINFO_BIOS	m_Bios;
};

//***************************************************************************
//
class CMainBoardInfo
{
public:
	CMainBoardInfo();
	~CMainBoardInfo();

	BOOL GetInformation(CWmi &Wmi);

	const TCHAR* GetDescription() const {
		return m_MainBoard.m_tszDescription;
	}
	const TCHAR* GetManufacturer() const {
		return m_MainBoard.m_tszManufacturer;
	}
	const TCHAR* GetProduct() const {
		return m_MainBoard.m_tszProduct;
	}
	const TCHAR* GetSerialNumber() const {
		return m_MainBoard.m_tszSerialNumber;
	}

private:
	HWINFO_MAINBOARD	m_MainBoard;
};

//***************************************************************************
//
class CMemoryInfo
{
public:
	CMemoryInfo();
	~CMemoryInfo();

	BOOL GetInformation(CWmi &Wmi);

	DWORD GetRamCount() const {
		return m_Memory.m_dwRamCount;
	}
	const __int64 GetTotalMemSize() const {
		return m_Memory.m_nTotalMemSize;
	}
	const __int64 GetPhysicalMemSize() const {
		return m_Memory.m_nPhysicalMemSize * 1024;
	}
	const __int64 GetUseMemSize() const {
		return m_Memory.m_nTotalMemSize - (m_Memory.m_nPhysicalMemSize * 1024);
	}
	const double GetPercentUsedRam() const {
		return (double)(m_Memory.m_nTotalMemSize - (m_Memory.m_nPhysicalMemSize * 1024)) / (double)m_Memory.m_nTotalMemSize;
	}
	const __int64 GetTotalVirtualMemSize() const {
		return m_Memory.m_nTotalVirtualMemSize * 1024;
	}
	const __int64 GetFreeVirtualMemSize() const {
		return m_Memory.m_nFreeVirtualMemSize * 1024;
	}
	const __int64 GetTotalPageFile() const {
		return m_Memory.m_nTotalPageFileSize * 1024;
	}
	const __int64 GetFreePageFile() const {
		return m_Memory.m_nFreePageFileSize * 1024;
	}
	CBaseLinkedList<HWINFO_RAM *>* GetRamArray() {
		return &m_sRamArray;
	}

private:
	void	FormFactorFormatDesc(DWORD dwFormFactor, TCHAR *ptszFormat) const;
	void	MemoryTypeFormatDesc(DWORD dwMemoryType, TCHAR *ptszMemoryType) const;

private:
	HWINFO_MEMORY	m_Memory;

	CBaseLinkedList<HWINFO_RAM *> m_sRamArray;
};

//***************************************************************************
//
class CHdDiskInfo
{
public:
	CHdDiskInfo();
	~CHdDiskInfo();

	BOOL GetInformation(CWmi &Wmi);

	CBaseLinkedList<HWINFO_HDDISK *>* GetHdDiskArray() {
		return &m_sHdDiskArray;
	}

private:
	CBaseLinkedList<HWINFO_HDDISK *> m_sHdDiskArray;
};

//***************************************************************************
//
class CDriveInfo
{
public:
	CDriveInfo();
	~CDriveInfo();

	BOOL GetInformation(CWmi &Wmi);

	DWORD GetDriveCount() const {
		return m_Drives.m_dwDriveCount;
	}
	const __int64 GetTotalSpaceSize() const {
		return m_Drives.m_nTotalSpace;
	}
	const __int64 GetFreeSpaceSize() const {
		return m_Drives.m_nFreeSpace;
	}
	const __int64 GetUsedSpaceSize() const {
		return m_Drives.m_nTotalSpace - m_Drives.m_nFreeSpace;
	}

	CBaseLinkedList<HWINFO_DRIVE *>* GetDriveArray() {
		return &m_sDriveArray;
	}

private:
	HWINFO_DRIVES	m_Drives;

	CBaseLinkedList<HWINFO_DRIVE *> m_sDriveArray;
};

//***************************************************************************
//
class CSoundCardInfo
{
public:
	CSoundCardInfo();
	~CSoundCardInfo();

	BOOL GetInformation();

	BOOL HasVolCtrl() const {
		return m_SoundCard.m_bHasVolCtrl;
	}
	BOOL HasSeparateLRVolCtrl() const {
		return m_SoundCard.m_bHasSeparateLRVolCtrl;
	}
	const TCHAR* GetProductName() const {
		return m_SoundCard.m_tszProductName;
	}
	const TCHAR* CSoundCardInfo::GetCompanyName() const {
		return m_SoundCard.m_tszCompanyName;
	}

private:
	void	GetAudioDevCompanyName(int nCompany, TCHAR *ptszCompany) const;

private:
	HWINFO_SOUNDCARD	m_SoundCard;
};

//***************************************************************************
//
class CVideoCardInfo
{
public:
	CVideoCardInfo();
	~CVideoCardInfo();

	BOOL GetInformation();

	CBaseLinkedList<HWINFO_VIDEOCARD *>* GetVideoCardArray() {
		return &m_sVideoCardArray;
	}

private:
	CBaseLinkedList<HWINFO_VIDEOCARD *> m_sVideoCardArray;
};

//***************************************************************************
//
class CNetworkCardInfo
{
public:
	CNetworkCardInfo();
	~CNetworkCardInfo();

	BOOL GetInformation(CWmi &Wmi);

	CBaseLinkedList<HWINFO_NETWORKCARD *>* GetNetworkCardArray() {
		return &m_sNetworkCardArray;
	}

private:
	CBaseLinkedList<HWINFO_NETWORKCARD *> m_sNetworkCardArray;
};

//***************************************************************************
//
class CCdromInfo
{
public:
	CCdromInfo();
	~CCdromInfo();

	BOOL GetInformation(CWmi &Wmi);

	CBaseLinkedList<HWINFO_CDROM *>* GetCdromArray() {
		return &m_sCdromArray;
	}

private:
	CBaseLinkedList<HWINFO_CDROM *> m_sCdromArray;
};

//***************************************************************************
//
class CKeyBoardInfo
{
public:
	CKeyBoardInfo();
	~CKeyBoardInfo();

	BOOL GetInformation(CWmi &Wmi);

	const TCHAR* GetDescription() const {
		return m_KeyBoard.m_tszDescription;
	}
	const TCHAR* GetType() const {
		return m_KeyBoard.m_tszType;
	}

private:
	void DetectKbType();

private:
	HWINFO_KEYBOARD	m_KeyBoard;
};

//***************************************************************************
//
class CMouseInfo
{
public:
	CMouseInfo();
	~CMouseInfo();

	BOOL GetInformation(CWmi &Wmi);

	const TCHAR* GetName() const {
		return m_Mouse.m_tszName;
	}
	const TCHAR* GetManufacturer() const {
		return m_Mouse.m_tszManufacturer;
	}
	const TCHAR* GetDescription() const {
		return m_Mouse.m_tszDescription;
	}

private:
	HWINFO_MOUSE	m_Mouse;
};

//***************************************************************************
//
class CMonitorInfo
{
public:
	CMonitorInfo();
	~CMonitorInfo();

	BOOL GetInformation(CWmi &Wmi);

	CBaseLinkedList<HWINFO_MONITOR *>* GetMonitorArray() {
		return &m_sMonitorArray;
	}

private:
	CBaseLinkedList<HWINFO_MONITOR *> m_sMonitorArray;
};

#endif // ndef __HARDWAREINFO_H__
