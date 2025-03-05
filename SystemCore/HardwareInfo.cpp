
//***************************************************************************
// HardwareInfo.cpp: implementation of the Hardware Information class.
//
//***************************************************************************

#include "pch.h"
#include "HardwareInfo.h"

//***************************************************************************
//
void ChangeDataFormat(const __int64& nData, TCHAR *ptszFormat)
{
	const int NUMFORMATTERS = 5;
	double	dblBase = (double)nData;
	int		nNumConversions = 0;
	TCHAR	tszFormatters[NUMFORMATTERS][10] = { _T(" bytes"), _T(" KB"), _T(" MB"), _T(" GB"), _T(" TB") };

	while( dblBase > 1000 )
	{
		dblBase /= 1024;
		nNumConversions++;
	}

	if( (0 <= nNumConversions) && (nNumConversions <= NUMFORMATTERS) )
		_stprintf_s(ptszFormat, NUMERIC_STRING_LEN, _T("%0.2f%s"), dblBase, tszFormatters[nNumConversions]);
}

//***************************************************************************
// Construction/Destruction
//***************************************************************************

CBiosInfo::CBiosInfo()
{
	ZeroMemory(&m_Bios, sizeof(HWINFO_BIOS));
}

CBiosInfo::~CBiosInfo()
{
}

//***************************************************************************
//
BOOL CBiosInfo::GetInformation(CWmi &Wmi)
{
	USES_CONVERSION;

	int		nIndex = 0;

	VARIANT		vtManufacturer;
	VARIANT		vtSmVersion;
	VARIANT		vtVersion;
	VARIANT		vtIdentificationCode;
	VARIANT		vtSerialNumber;
	VARIANT		vtReleaseDate;

	nIndex = Wmi.ExecQuery(_T("Win32_BIOS"));
	if( nIndex < 0 ) return false;

	VariantInit(&vtManufacturer);

	Wmi.GetProperties(0, _T("Manufacturer"), vtManufacturer);

	if( vtManufacturer.vt == VT_BSTR )
	{
#ifdef _UNICODE	
		_tcscpy_s(m_Bios.m_tszManufacturer, _countof(m_Bios.m_tszManufacturer), OLE2W(vtManufacturer.bstrVal));
#else
		_tcscpy_s(m_Bios.m_tszManufacturer, _countof(m_Bios.m_tszManufacturer), OLE2A(vtManufacturer.bstrVal));
#endif
	}

	VariantClear(&vtManufacturer);

	VariantInit(&vtSmVersion);

	Wmi.GetProperties(0, _T("SMBIOSBIOSVersion"), vtSmVersion);

	if( vtSmVersion.vt == VT_BSTR )
	{
#ifdef _UNICODE	
		_tcscpy_s(m_Bios.m_tszSmVersion, _countof(m_Bios.m_tszSmVersion), OLE2W(vtSmVersion.bstrVal));
#else
		_tcscpy_s(m_Bios.m_tszSmVersion, _countof(m_Bios.m_tszSmVersion), OLE2A(vtSmVersion.bstrVal));
#endif
	}

	VariantClear(&vtSmVersion);

	VariantInit(&vtVersion);

	Wmi.GetProperties(0, _T("Version"), vtVersion);

	if( vtVersion.vt == VT_BSTR )
	{
#ifdef _UNICODE	
		_tcscpy_s(m_Bios.m_tszVersion, _countof(m_Bios.m_tszVersion), OLE2W(vtVersion.bstrVal));
#else
		_tcscpy_s(m_Bios.m_tszVersion, _countof(m_Bios.m_tszVersion), OLE2A(vtVersion.bstrVal));
#endif
	}

	VariantClear(&vtVersion);

	VariantInit(&vtIdentificationCode);

	Wmi.GetProperties(0, _T("IdentificationCode"), vtIdentificationCode);

	if( vtIdentificationCode.vt == VT_BSTR )
	{
#ifdef _UNICODE	
		_tcscpy_s(m_Bios.m_tszIdentificationCode, _countof(m_Bios.m_tszIdentificationCode), OLE2W(vtIdentificationCode.bstrVal));
#else
		_tcscpy_s(m_Bios.m_tszIdentificationCode, _countof(m_Bios.m_tszIdentificationCode), OLE2A(vtIdentificationCode.bstrVal));
#endif
	}

	VariantClear(&vtIdentificationCode);

	VariantInit(&vtSerialNumber);

	Wmi.GetProperties(0, _T("SerialNumber"), vtSerialNumber);

	if( vtSerialNumber.vt == VT_BSTR )
	{
#ifdef _UNICODE	
		_tcscpy_s(m_Bios.m_tszSerialNumber, _countof(m_Bios.m_tszSerialNumber), OLE2W(vtSerialNumber.bstrVal));
#else
		_tcscpy_s(m_Bios.m_tszSerialNumber, _countof(m_Bios.m_tszSerialNumber), OLE2A(vtSerialNumber.bstrVal));
#endif
	}

	VariantClear(&vtSerialNumber);

	VariantInit(&vtReleaseDate);

	Wmi.GetProperties(0, _T("ReleaseDate"), vtReleaseDate);

	if( vtReleaseDate.vt == VT_BSTR )
	{
#ifdef _UNICODE	
		_tcscpy_s(m_Bios.m_tszReleaseDate, _countof(m_Bios.m_tszReleaseDate), OLE2W(vtReleaseDate.bstrVal));
#else
		_tcscpy_s(m_Bios.m_tszReleaseDate, _countof(m_Bios.m_tszReleaseDate), OLE2A(vtReleaseDate.bstrVal));
#endif
	}

	VariantClear(&vtReleaseDate);

	return true;
}

///***************************************************************************
// Construction/Destruction
//***************************************************************************

CMainBoardInfo::CMainBoardInfo()
{
	ZeroMemory(&m_MainBoard, sizeof(HWINFO_MAINBOARD));
}

CMainBoardInfo::~CMainBoardInfo()
{
}

//***************************************************************************
//
BOOL CMainBoardInfo::GetInformation(CWmi &Wmi)
{
	USES_CONVERSION;

	int			nIndex = 0;

	VARIANT		vtProduct;
	VARIANT		vtSerialNumber;
	VARIANT		vtManufacturer;
	VARIANT		vtDescription;

	nIndex = Wmi.ExecQuery(_T("Win32_BaseBoard"));
	if( nIndex < 0 ) return false;

	VariantInit(&vtProduct);

	Wmi.GetProperties(0, _T("Product"), vtProduct);

	if( vtProduct.vt == VT_BSTR )
	{
#ifdef _UNICODE	
		_tcscpy_s(m_MainBoard.m_tszProduct, _countof(m_MainBoard.m_tszProduct), OLE2W(vtProduct.bstrVal));
#else
		_tcscpy_s(m_MainBoard.m_tszProduct, _countof(m_MainBoard.m_tszProduct), OLE2A(vtProduct.bstrVal));
#endif
	}

	VariantClear(&vtProduct);

	VariantInit(&vtSerialNumber);

	Wmi.GetProperties(0, _T("SerialNumber"), vtSerialNumber);

	if( vtSerialNumber.vt == VT_BSTR )
	{
#ifdef _UNICODE	
		_tcscpy_s(m_MainBoard.m_tszSerialNumber, _countof(m_MainBoard.m_tszSerialNumber), OLE2W(vtSerialNumber.bstrVal));
#else
		_tcscpy_s(m_MainBoard.m_tszSerialNumber, _countof(m_MainBoard.m_tszSerialNumber), OLE2A(vtSerialNumber.bstrVal));
#endif
	}

	VariantClear(&vtSerialNumber);

	VariantInit(&vtManufacturer);

	Wmi.GetProperties(0, _T("Manufacturer"), vtManufacturer);

	if( vtManufacturer.vt == VT_BSTR )
	{
#ifdef _UNICODE	
		_tcscpy_s(m_MainBoard.m_tszManufacturer, _countof(m_MainBoard.m_tszManufacturer), OLE2W(vtManufacturer.bstrVal));
#else
		_tcscpy_s(m_MainBoard.m_tszManufacturer, _countof(m_MainBoard.m_tszManufacturer), OLE2A(vtManufacturer.bstrVal));
#endif
	}

	VariantClear(&vtManufacturer);

	VariantInit(&vtDescription);

	Wmi.GetProperties(0, _T("Description"), vtDescription);

	if( vtDescription.vt == VT_BSTR )
	{
#ifdef _UNICODE	
		_tcscpy_s(m_MainBoard.m_tszDescription, _countof(m_MainBoard.m_tszDescription), OLE2W(vtDescription.bstrVal));
#else
		_tcscpy_s(m_MainBoard.m_tszDescription, _countof(m_MainBoard.m_tszDescription), OLE2A(vtDescription.bstrVal));
#endif
	}

	VariantClear(&vtDescription);

	return true;
}

//***************************************************************************
// Construction/Destruction
//***************************************************************************

CMemoryInfo::CMemoryInfo()
{
	ZeroMemory(&m_Memory, sizeof(HWINFO_MEMORY));
}

CMemoryInfo::~CMemoryInfo()
{
	HWINFO_RAM		*pRam = NULL;

	for( int i = 0; i < m_sRamArray.GetCount(); i++ )
	{
		pRam = m_sRamArray.At(i);

		if( pRam )
		{
			delete pRam;
			pRam = NULL;
		}
	}
}

//***************************************************************************
//
BOOL CMemoryInfo::GetInformation(CWmi &Wmi)
{
	USES_CONVERSION;

	DWORD	dwRamCount = 0;
	int		nIndex = 0;
	TCHAR	tszCapacity[NUMERIC_STRING_LEN];
	TCHAR	tszPhysicalMemory[NUMERIC_STRING_LEN];
	TCHAR	tszTotalVirtualMemory[NUMERIC_STRING_LEN];
	TCHAR	tszFreeVirtualMemory[NUMERIC_STRING_LEN];
	TCHAR	tszTotalPageFile[NUMERIC_STRING_LEN];
	TCHAR	tszFreePageFile[NUMERIC_STRING_LEN];

	VARIANT		vtBankLabel;
	VARIANT		vtName;
	VARIANT		vtDeviceLocator;
	VARIANT		vtCapacity;
	VARIANT		vtFormFactor;
	VARIANT		vtMemoryType;
	VARIANT		vtSpeed;
	VARIANT		vtFreePhysicalMemory;
	VARIANT		vtTotalVirtualMemory;
	VARIANT		vtFreeVirtualMemory;
	VARIANT		vtTotalPageFile;
	VARIANT		vtFreePageFile;

	HWINFO_RAM	*pRam = NULL;

	nIndex = Wmi.ExecQuery(_T("Win32_PhysicalMemory"));
	if( nIndex < 0 ) return false;

	for( int i = 0; i < nIndex; i++ )
	{
		pRam = new HWINFO_RAM;

		VariantInit(&vtBankLabel);

		Wmi.GetProperties(i, _T("BankLabel"), vtBankLabel);

		if( vtBankLabel.vt == VT_BSTR )
		{
#ifdef _UNICODE	
			_tcscpy_s(pRam->m_tszBankLabel, _countof(pRam->m_tszBankLabel), OLE2W(vtBankLabel.bstrVal));
#else
			_tcscpy_s(pRam->m_tszBankLabel, _countof(pRam->m_tszBankLabel), OLE2A(vtBankLabel.bstrVal));
#endif
		}

		VariantClear(&vtBankLabel);

		VariantInit(&vtName);

		Wmi.GetProperties(i, _T("Name"), vtName);

		if( vtName.vt == VT_BSTR )
		{
#ifdef _UNICODE	
			_tcscpy_s(pRam->m_tszName, _countof(pRam->m_tszName), OLE2W(vtName.bstrVal));
#else
			_tcscpy_s(pRam->m_tszName, _countof(pRam->m_tszName), OLE2A(vtName.bstrVal));
#endif
		}

		VariantClear(&vtName);

		VariantInit(&vtDeviceLocator);

		Wmi.GetProperties(i, _T("DeviceLocator"), vtDeviceLocator);

		if( vtDeviceLocator.vt == VT_BSTR )
		{
#ifdef _UNICODE	
			_tcscpy_s(pRam->m_tszDeviceLocator, _countof(pRam->m_tszDeviceLocator), OLE2W(vtDeviceLocator.bstrVal));
#else
			_tcscpy_s(pRam->m_tszDeviceLocator, _countof(pRam->m_tszDeviceLocator), OLE2A(vtDeviceLocator.bstrVal));
#endif
		}

		VariantClear(&vtDeviceLocator);

		VariantInit(&vtCapacity);

		Wmi.GetProperties(i, _T("Capacity"), vtCapacity);

		if( vtCapacity.vt == VT_BSTR )
		{
#ifdef _UNICODE	
			_tcscpy_s(tszCapacity, _countof(tszCapacity), OLE2W(vtCapacity.bstrVal));
#else
			_tcscpy_s(tszCapacity, _countof(tszCapacity), OLE2A(vtCapacity.bstrVal));
#endif

			if( IsAllNumeric(tszCapacity) )
			{
				pRam->m_nCapacity = _ttoi64(tszCapacity);
				m_Memory.m_nTotalMemSize = m_Memory.m_nTotalMemSize + _ttoi64(tszCapacity);
			}
		}

		VariantClear(&vtCapacity);

		VariantInit(&vtFormFactor);

		Wmi.GetProperties(i, _T("FormFactor"), vtFormFactor);

		if( vtFormFactor.vt == VT_I4 )
		{
			pRam->m_dwFormFactor = vtFormFactor.lVal;
			FormFactorFormatDesc(vtFormFactor.lVal, pRam->m_tszFormFactorDesc);
		}

		VariantClear(&vtFormFactor);

		VariantInit(&vtMemoryType);

		Wmi.GetProperties(i, _T("MemoryType"), vtMemoryType);

		if( vtMemoryType.vt == VT_I4 )
		{
			pRam->m_dwMemoryType = vtMemoryType.lVal;
			MemoryTypeFormatDesc(vtMemoryType.lVal, pRam->m_tszMemoryTypeDesc);
		}

		VariantClear(&vtMemoryType);

		VariantInit(&vtSpeed);

		Wmi.GetProperties(i, _T("Speed"), vtSpeed);

		if( vtSpeed.vt == VT_I4 )
			pRam->m_dwSpeed = vtSpeed.lVal;

		VariantClear(&vtSpeed);

		dwRamCount++;

		m_sRamArray.Add(pRam);
	}

	m_Memory.m_dwRamCount = dwRamCount;

	nIndex = Wmi.ExecQuery(_T("Win32_OperatingSystem"));
	if( nIndex < 0 ) return false;

	VariantInit(&vtFreePhysicalMemory);

	Wmi.GetProperties(0, _T("FreePhysicalMemory"), vtFreePhysicalMemory);

	if( vtFreePhysicalMemory.vt == VT_BSTR )
	{
#ifdef _UNICODE	
		_tcscpy_s(tszPhysicalMemory, _countof(tszPhysicalMemory), OLE2W(vtFreePhysicalMemory.bstrVal));
#else
		_tcscpy_s(tszPhysicalMemory, _countof(tszPhysicalMemory), OLE2A(vtFreePhysicalMemory.bstrVal));
#endif

		if( IsAllNumeric(tszPhysicalMemory) )
			m_Memory.m_nPhysicalMemSize = m_Memory.m_nPhysicalMemSize + _ttoi64(tszPhysicalMemory);
	}

	VariantClear(&vtFreePhysicalMemory);

	VariantInit(&vtTotalVirtualMemory);

	Wmi.GetProperties(0, _T("TotalVirtualMemorySize"), vtTotalVirtualMemory);

	if( vtTotalVirtualMemory.vt == VT_BSTR )
	{
#ifdef _UNICODE	
		_tcscpy_s(tszTotalVirtualMemory, _countof(tszTotalVirtualMemory), OLE2W(vtTotalVirtualMemory.bstrVal));
#else
		_tcscpy_s(tszTotalVirtualMemory, _countof(tszTotalVirtualMemory), OLE2A(vtTotalVirtualMemory.bstrVal));
#endif

		if( IsAllNumeric(tszTotalVirtualMemory) )
			m_Memory.m_nTotalVirtualMemSize = m_Memory.m_nTotalVirtualMemSize + _ttoi64(tszTotalVirtualMemory);
	}

	VariantClear(&vtTotalVirtualMemory);

	VariantInit(&vtFreeVirtualMemory);

	Wmi.GetProperties(0, _T("FreeVirtualMemory"), vtFreeVirtualMemory);

	if( vtFreeVirtualMemory.vt == VT_BSTR )
	{
#ifdef _UNICODE	
		_tcscpy_s(tszFreeVirtualMemory, _countof(tszFreeVirtualMemory), OLE2W(vtFreeVirtualMemory.bstrVal));
#else
		_tcscpy_s(tszFreeVirtualMemory, _countof(tszFreeVirtualMemory), OLE2A(vtFreeVirtualMemory.bstrVal));
#endif

		if( IsAllNumeric(tszFreeVirtualMemory) )
			m_Memory.m_nFreeVirtualMemSize = m_Memory.m_nFreeVirtualMemSize + _ttoi64(tszFreeVirtualMemory);
	}

	VariantClear(&vtFreeVirtualMemory);

	VariantInit(&vtTotalPageFile);

	Wmi.GetProperties(0, _T("SizeStoredInPagingFiles"), vtTotalPageFile);

	if( vtTotalPageFile.vt == VT_BSTR )
	{
#ifdef _UNICODE	
		_tcscpy_s(tszTotalPageFile, _countof(tszTotalPageFile), OLE2W(vtTotalPageFile.bstrVal));
#else
		_tcscpy_s(tszTotalPageFile, _countof(tszTotalPageFile), OLE2A(vtTotalPageFile.bstrVal));
#endif

		if( IsAllNumeric(tszTotalPageFile) )
			m_Memory.m_nTotalPageFileSize = m_Memory.m_nTotalPageFileSize + _ttoi64(tszTotalPageFile);
	}

	VariantClear(&vtTotalPageFile);

	VariantInit(&vtFreePageFile);

	Wmi.GetProperties(0, _T("FreeSpaceInPagingFiles"), vtFreePageFile);

	if( vtFreePageFile.vt == VT_BSTR )
	{
#ifdef _UNICODE	
		_tcscpy_s(tszFreePageFile, _countof(tszFreePageFile), OLE2W(vtFreePageFile.bstrVal));
#else
		_tcscpy_s(tszFreePageFile, _countof(tszFreePageFile), OLE2A(vtFreePageFile.bstrVal));
#endif

		if( IsAllNumeric(tszFreePageFile) )
			m_Memory.m_nFreePageFileSize = m_Memory.m_nFreePageFileSize + _ttoi64(tszFreePageFile);
	}

	VariantClear(&vtFreePageFile);

	return true;
}

//***************************************************************************
//
void CMemoryInfo::FormFactorFormatDesc(DWORD dwFormFactor, TCHAR *ptszFormFactor) const
{
	switch( dwFormFactor )
	{
		case 0:
			_tcscpy_s(ptszFormFactor, RAM_FORMFACTORDESC_STRLEN, _T("Unknown"));
			break;
		case 1:
			_tcscpy_s(ptszFormFactor, RAM_FORMFACTORDESC_STRLEN, _T("Other"));
			break;
		case 2:
			_tcscpy_s(ptszFormFactor, RAM_FORMFACTORDESC_STRLEN, _T("SIP"));
			break;
		case 3:
			_tcscpy_s(ptszFormFactor, RAM_FORMFACTORDESC_STRLEN, _T("DIP"));
			break;
		case 4:
			_tcscpy_s(ptszFormFactor, RAM_FORMFACTORDESC_STRLEN, _T("ZIP"));
			break;
		case 5:
			_tcscpy_s(ptszFormFactor, RAM_FORMFACTORDESC_STRLEN, _T("SOJ"));
			break;
		case 6:
			_tcscpy_s(ptszFormFactor, RAM_FORMFACTORDESC_STRLEN, _T("Proprietary"));
			break;
		case 7:
			_tcscpy_s(ptszFormFactor, RAM_FORMFACTORDESC_STRLEN, _T("SIMM"));
			break;
		case 8:
			_tcscpy_s(ptszFormFactor, RAM_FORMFACTORDESC_STRLEN, _T("DIMM"));
			break;
		case 9:
			_tcscpy_s(ptszFormFactor, RAM_FORMFACTORDESC_STRLEN, _T("TSOP"));
			break;
		case 10:
			_tcscpy_s(ptszFormFactor, RAM_FORMFACTORDESC_STRLEN, _T("PGA"));
			break;
		case 11:
			_tcscpy_s(ptszFormFactor, RAM_FORMFACTORDESC_STRLEN, _T("RIMM"));
			break;
		case 12:
			_tcscpy_s(ptszFormFactor, RAM_FORMFACTORDESC_STRLEN, _T("SODIMM"));
			break;
		case 13:
			_tcscpy_s(ptszFormFactor, RAM_FORMFACTORDESC_STRLEN, _T("SRIMM"));
			break;
		case 14:
			_tcscpy_s(ptszFormFactor, RAM_FORMFACTORDESC_STRLEN, _T("SMD"));
			break;
		case 15:
			_tcscpy_s(ptszFormFactor, RAM_FORMFACTORDESC_STRLEN, _T("SSMP"));
			break;
		case 16:
			_tcscpy_s(ptszFormFactor, RAM_FORMFACTORDESC_STRLEN, _T("QFP"));
			break;
		case 17:
			_tcscpy_s(ptszFormFactor, RAM_FORMFACTORDESC_STRLEN, _T("TQFP"));
			break;
		case 18:
			_tcscpy_s(ptszFormFactor, RAM_FORMFACTORDESC_STRLEN, _T("SOIC"));
			break;
		case 19:
			_tcscpy_s(ptszFormFactor, RAM_FORMFACTORDESC_STRLEN, _T("LCC"));
			break;
		case 20:
			_tcscpy_s(ptszFormFactor, RAM_FORMFACTORDESC_STRLEN, _T("PLCC"));
			break;
		case 21:
			_tcscpy_s(ptszFormFactor, RAM_FORMFACTORDESC_STRLEN, _T("BGA"));
			break;
		case 22:
			_tcscpy_s(ptszFormFactor, RAM_FORMFACTORDESC_STRLEN, _T("FPBGA"));
			break;
		case 23:
			_tcscpy_s(ptszFormFactor, RAM_FORMFACTORDESC_STRLEN, _T("LGA"));
			break;
		default:
			ptszFormFactor[0] = '\0';
			break;
	}
}

//***************************************************************************
//
void CMemoryInfo::MemoryTypeFormatDesc(DWORD dwMemoryType, TCHAR *ptszMemoryType) const
{
	switch( dwMemoryType )
	{
		case 0:
			_tcscpy_s(ptszMemoryType, RAM_MEMORYTYPEDESC_STRLEN, _T("Unknown"));
			break;
		case 1:
			_tcscpy_s(ptszMemoryType, RAM_MEMORYTYPEDESC_STRLEN, _T("Other"));
			break;
		case 2:
			_tcscpy_s(ptszMemoryType, RAM_MEMORYTYPEDESC_STRLEN, _T("DRAM"));
			break;
		case 3:
			_tcscpy_s(ptszMemoryType, RAM_MEMORYTYPEDESC_STRLEN, _T("Synchronous DRAM"));
			break;
		case 4:
			_tcscpy_s(ptszMemoryType, RAM_MEMORYTYPEDESC_STRLEN, _T("Cache DRAM"));
			break;
		case 5:
			_tcscpy_s(ptszMemoryType, RAM_MEMORYTYPEDESC_STRLEN, _T("EDO"));
			break;
		case 6:
			_tcscpy_s(ptszMemoryType, RAM_MEMORYTYPEDESC_STRLEN, _T("EDRAM"));
			break;
		case 7:
			_tcscpy_s(ptszMemoryType, RAM_MEMORYTYPEDESC_STRLEN, _T("VRAM"));
			break;
		case 8:
			_tcscpy_s(ptszMemoryType, RAM_MEMORYTYPEDESC_STRLEN, _T("SRAM"));
			break;
		case 9:
			_tcscpy_s(ptszMemoryType, RAM_MEMORYTYPEDESC_STRLEN, _T("RAM"));
			break;
		case 10:
			_tcscpy_s(ptszMemoryType, RAM_MEMORYTYPEDESC_STRLEN, _T("ROM"));
			break;
		case 11:
			_tcscpy_s(ptszMemoryType, RAM_MEMORYTYPEDESC_STRLEN, _T("Flash"));
			break;
		case 12:
			_tcscpy_s(ptszMemoryType, RAM_MEMORYTYPEDESC_STRLEN, _T("EEPROM"));
			break;
		case 13:
			_tcscpy_s(ptszMemoryType, RAM_MEMORYTYPEDESC_STRLEN, _T("FEPROM"));
			break;
		case 14:
			_tcscpy_s(ptszMemoryType, RAM_MEMORYTYPEDESC_STRLEN, _T("EPROM"));
			break;
		case 15:
			_tcscpy_s(ptszMemoryType, RAM_MEMORYTYPEDESC_STRLEN, _T("CDRAM"));
			break;
		case 16:
			_tcscpy_s(ptszMemoryType, RAM_MEMORYTYPEDESC_STRLEN, _T("3DRAM"));
			break;
		case 17:
			_tcscpy_s(ptszMemoryType, RAM_MEMORYTYPEDESC_STRLEN, _T("SDRAM"));
			break;
		case 18:
			_tcscpy_s(ptszMemoryType, RAM_MEMORYTYPEDESC_STRLEN, _T("SGRAM"));
			break;
		case 19:
			_tcscpy_s(ptszMemoryType, RAM_MEMORYTYPEDESC_STRLEN, _T("RDRAM"));
			break;
		case 20:
			_tcscpy_s(ptszMemoryType, RAM_MEMORYTYPEDESC_STRLEN, _T("DDR"));
			break;
		case 21:
			_tcscpy_s(ptszMemoryType, RAM_MEMORYTYPEDESC_STRLEN, _T("DDR-2"));
			break;
		default:
			ptszMemoryType[0] = '\0';
			break;
	}
}

//***************************************************************************
// Construction/Destruction
//***************************************************************************

CHdDiskInfo::CHdDiskInfo()
{
}

CHdDiskInfo::~CHdDiskInfo()
{
	HWINFO_HDDISK		*pHdDisk = NULL;

	for( int i = 0; i < m_sHdDiskArray.GetCount(); i++ )
	{
		pHdDisk = m_sHdDiskArray.At(i);

		if( pHdDisk )
		{
			delete pHdDisk;
			pHdDisk = NULL;
		}
	}
}

//***************************************************************************
//
BOOL CHdDiskInfo::GetInformation(CWmi &Wmi)
{
	USES_CONVERSION;

	int		nIndex = 0;
	TCHAR	tszTotalSize[NUMERIC_STRING_LEN];

	VARIANT		vtModel;
	VARIANT		vtName;
	VARIANT		vtManufacturer;
	VARIANT		vtDescription;
	VARIANT		vtSize;

	HWINFO_HDDISK	*pHdDisk = NULL;

	nIndex = Wmi.ExecQuery(_T("Win32_DiskDrive"));
	if( nIndex < 0 ) return false;

	for( int i = 0; i < nIndex; i++ )
	{
		pHdDisk = new HWINFO_HDDISK;

		VariantInit(&vtModel);

		Wmi.GetProperties(i, _T("Model"), vtModel);

		if( vtModel.vt == VT_BSTR )
		{
#ifdef _UNICODE	
			_tcscpy_s(pHdDisk->m_tszModel, _countof(pHdDisk->m_tszModel), OLE2W(vtModel.bstrVal));
#else
			_tcscpy_s(pHdDisk->m_tszModel, _countof(pHdDisk->m_tszModel), OLE2A(vtModel.bstrVal));
#endif
		}

		VariantClear(&vtModel);

		VariantInit(&vtName);

		Wmi.GetProperties(i, _T("Name"), vtName);

		if( vtName.vt == VT_BSTR )
		{
#ifdef _UNICODE	
			_tcscpy_s(pHdDisk->m_tszName, _countof(pHdDisk->m_tszName), OLE2W(vtName.bstrVal));
#else
			_tcscpy_s(pHdDisk->m_tszName, _countof(pHdDisk->m_tszName), OLE2A(vtName.bstrVal));
#endif
		}

		VariantClear(&vtName);

		VariantInit(&vtManufacturer);

		Wmi.GetProperties(i, _T("Manufacturer"), vtManufacturer);

		if( vtManufacturer.vt == VT_BSTR )
		{
#ifdef _UNICODE	
			_tcscpy_s(pHdDisk->m_tszManufacturer, _countof(pHdDisk->m_tszManufacturer), OLE2W(vtManufacturer.bstrVal));
#else
			_tcscpy_s(pHdDisk->m_tszManufacturer, _countof(pHdDisk->m_tszManufacturer), OLE2A(vtManufacturer.bstrVal));
#endif
		}

		VariantClear(&vtManufacturer);

		VariantInit(&vtDescription);

		Wmi.GetProperties(i, _T("Description"), vtDescription);

		if( vtDescription.vt == VT_BSTR )
		{
#ifdef _UNICODE	
			_tcscpy_s(pHdDisk->m_tszDescription, _countof(pHdDisk->m_tszDescription), OLE2W(vtDescription.bstrVal));
#else
			_tcscpy_s(pHdDisk->m_tszDescription, _countof(pHdDisk->m_tszDescription), OLE2A(vtDescription.bstrVal));
#endif
		}

		VariantClear(&vtDescription);

		VariantInit(&vtSize);

		Wmi.GetProperties(i, _T("Size"), vtSize);

		if( vtSize.vt == VT_BSTR )
		{
#ifdef _UNICODE	
			_tcscpy_s(tszTotalSize, _countof(tszTotalSize), OLE2W(vtSize.bstrVal));
#else
			_tcscpy_s(tszTotalSize, _countof(tszTotalSize), OLE2A(vtSize.bstrVal));
#endif

			if( IsAllNumeric(tszTotalSize) )
				pHdDisk->m_nTotalSize = _ttoi64(tszTotalSize);
		}

		VariantClear(&vtSize);

		m_sHdDiskArray.Add(pHdDisk);
	}

	return true;
}

//***************************************************************************
// Construction/Destruction
//***************************************************************************

CDriveInfo::CDriveInfo()
{
}

CDriveInfo::~CDriveInfo()
{
	HWINFO_DRIVE		*pDrive = NULL;

	for( int i = 0; i < m_sDriveArray.GetCount(); i++ )
	{
		pDrive = m_sDriveArray.At(i);

		if( pDrive )
		{
			delete pDrive;
			pDrive = NULL;
		}
	}
}

//***************************************************************************
//
BOOL CDriveInfo::GetInformation(CWmi &Wmi)
{
	USES_CONVERSION;

	DWORD	dwDriveCount = 0;
	int		nIndex = 0;
	TCHAR	tszTotalSpace[NUMERIC_STRING_LEN];
	TCHAR	tszFreeSpace[NUMERIC_STRING_LEN];

	VARIANT		vtName;
	VARIANT		vtFileSystem;
	VARIANT		vtSize;
	VARIANT		vtFreeSpace;

	HWINFO_DRIVE	*pDrive = NULL;

	nIndex = Wmi.ExecQuery(_T("Win32_LogicalDisk WHERE drivetype = 3"));
	if( nIndex < 0 ) return false;

	for( int i = 0; i < nIndex; i++ )
	{
		pDrive = new HWINFO_DRIVE;

		VariantInit(&vtName);

		Wmi.GetProperties(i, _T("Name"), vtName);

		if( vtName.vt == VT_BSTR )
		{
#ifdef _UNICODE	
			_tcscpy_s(pDrive->m_tszName, _countof(pDrive->m_tszName), OLE2W(vtName.bstrVal));
#else
			_tcscpy_s(pDrive->m_tszName, _countof(pDrive->m_tszName), OLE2A(vtName.bstrVal));
#endif
		}

		VariantClear(&vtName);

		VariantInit(&vtFileSystem);

		Wmi.GetProperties(i, _T("FileSystem"), vtFileSystem);

		if( vtFileSystem.vt == VT_BSTR )
		{
#ifdef _UNICODE	
			_tcscpy_s(pDrive->m_tszFileSystem, _countof(pDrive->m_tszFileSystem), OLE2W(vtFileSystem.bstrVal));
#else
			_tcscpy_s(pDrive->m_tszFileSystem, _countof(pDrive->m_tszFileSystem), OLE2A(vtFileSystem.bstrVal));
#endif
		}

		VariantClear(&vtFileSystem);

		VariantInit(&vtSize);

		Wmi.GetProperties(i, _T("Size"), vtSize);

		if( vtSize.vt == VT_BSTR )
		{
#ifdef _UNICODE	
			_tcscpy_s(tszTotalSpace, _countof(tszTotalSpace), OLE2W(vtSize.bstrVal));
#else
			_tcscpy_s(tszTotalSpace, _countof(tszTotalSpace), OLE2A(vtSize.bstrVal));
#endif

			if( IsAllNumeric(tszTotalSpace) )
			{
				pDrive->m_nTotalSpace = _ttoi64(tszTotalSpace);
				m_Drives.m_nTotalSpace = m_Drives.m_nTotalSpace + _ttoi64(tszTotalSpace);
			}
		}

		VariantClear(&vtSize);

		VariantInit(&vtFreeSpace);

		Wmi.GetProperties(i, _T("FreeSpace"), vtFreeSpace);

		if( vtFreeSpace.vt == VT_BSTR )
		{
#ifdef _UNICODE	
			_tcscpy_s(tszFreeSpace, _countof(tszFreeSpace), OLE2W(vtFreeSpace.bstrVal));
#else
			_tcscpy_s(tszFreeSpace, _countof(tszFreeSpace), OLE2A(vtFreeSpace.bstrVal));
#endif

			if( IsAllNumeric(tszFreeSpace) )
			{
				pDrive->m_nFreeSpace = _ttoi64(tszFreeSpace);
				m_Drives.m_nFreeSpace = m_Drives.m_nFreeSpace + _ttoi64(tszFreeSpace);
			}
		}

		VariantClear(&vtFreeSpace);

		dwDriveCount++;

		m_sDriveArray.Add(pDrive);
	}

	m_Drives.m_dwDriveCount = dwDriveCount;

	return true;
}

//***************************************************************************
// Construction/Destruction
//***************************************************************************

CSoundCardInfo::CSoundCardInfo()
{
	ZeroMemory(&m_SoundCard, sizeof(HWINFO_MEMORY));
}

CSoundCardInfo::~CSoundCardInfo()
{
}

//***************************************************************************
//
BOOL CSoundCardInfo::GetInformation()
{
	BOOL	bIsInstalled = false;
	UINT	uWavNumDevices = 0;
	UINT	uWavCaps = 0;
	TCHAR	tszCompany[SOUNDCARD_COMPANYNAME_STRLEN];

	WAVEOUTCAPS wavCaps;

	// Get the number of audio output devices installed on your system.
	uWavNumDevices = waveOutGetNumDevs();
	bIsInstalled = (uWavNumDevices > 0) ? TRUE : FALSE;

	// If the device is present, only then we will proceed to get other information.
 	if( bIsInstalled )
	{
		uWavCaps = sizeof(WAVEOUTCAPS);

		if( waveOutGetDevCaps(0, &wavCaps, uWavCaps) == MMSYSERR_NOERROR )
		{
			_tcscpy_s(m_SoundCard.m_tszProductName, _countof(m_SoundCard.m_tszProductName), wavCaps.szPname);

			GetAudioDevCompanyName(wavCaps.wMid, tszCompany);
			_tcscpy_s(m_SoundCard.m_tszCompanyName, _countof(m_SoundCard.m_tszCompanyName), tszCompany);

			m_SoundCard.m_bHasSeparateLRVolCtrl = (wavCaps.dwSupport & WAVECAPS_VOLUME) ? TRUE : FALSE;
			m_SoundCard.m_bHasVolCtrl = (wavCaps.dwSupport & AUXCAPS_VOLUME) ? TRUE : FALSE;
		}
	}

	return bIsInstalled;
}

//***************************************************************************
//
void CSoundCardInfo::GetAudioDevCompanyName(int nCompany, TCHAR *ptszCompany) const
{
	switch( nCompany )
	{
		case MM_MICROSOFT:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Microsoft Corporation"));
			break;
		case MM_CREATIVE:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Creative Labs, Inc"));
			break;
		case MM_MEDIAVISION:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Media Vision, Inc."));
			break;
		case MM_FUJITSU:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Fujitsu Corp."));
			break;
		case MM_ARTISOFT:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Artisoft, Inc."));
			break;
		case MM_TURTLE_BEACH:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Turtle Beach, Inc."));
			break;
		case MM_IBM:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("IBM Corporation"));
			break;
		case MM_VOCALTEC:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Vocaltec LTD."));
			break;
		case MM_ROLAND:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Roland"));
			break;
		case MM_DSP_SOLUTIONS:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("DSP Solutions, Inc."));
			break;
		case MM_NEC:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("NEC"));
			break;
		case MM_ATI:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("ATI"));
			break;
		case MM_WANGLABS:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Wang Laboratories, Inc"));
			break;
		case MM_TANDY:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Tandy Corporation"));
			break;
		case MM_VOYETRA:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Voyetra"));
			break;
		case MM_ANTEX:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Antex Electronics Corporation"));
			break;
		case MM_ICL_PS:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("ICL Personal Systems"));
			break;
		case MM_INTEL:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Intel Corporation"));
			break;
		case MM_GRAVIS:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Advanced Gravis"));
			break;
		case MM_VAL:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Video Associates Labs, Inc."));
			break;
		case MM_INTERACTIVE:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("InterActive Inc"));
			break;
		case MM_YAMAHA:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Yamaha Corporation of America"));
			break;
		case MM_EVEREX:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Everex Systems, Inc"));
			break;
		case MM_ECHO:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Echo Speech Corporation"));
			break;
		case MM_SIERRA:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Sierra Semiconductor Corp"));
			break;
		case MM_CAT:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Computer Aided Technologies"));
			break;
		case MM_APPS:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("APPS Software International"));
			break;
		case MM_DSP_GROUP:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("DSP Group, Inc"));
			break;
		case MM_MELABS:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("MicroEngineering Labs"));
			break;
		case MM_COMPUTER_FRIENDS:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Computer Friends, Inc."));
			break;
		case MM_ESS:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("ESS Technology"));
			break;
		case MM_AUDIOFILE:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Audio, Inc."));
			break;
		case MM_MOTOROLA:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Motorola, Inc."));
		case MM_CANOPUS:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Canopus, co., Ltd."));
			break;
		case MM_EPSON:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Seiko Epson Corporation"));
			break;
		case MM_TRUEVISION:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Truevision"));
			break;
		case MM_AZTECH:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Aztech Labs, Inc."));
			break;
		case MM_VIDEOLOGIC:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Videologic"));
			break;
		case MM_SCALACS:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("SCALACS"));
			break;
		case MM_KORG:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Korg Inc."));
			break;
		case MM_APT:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Audio Processing Technology"));
			break;
		case MM_ICS:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Integrated Circuit Systems, Inc."));
			break;
		case MM_ITERATEDSYS:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Iterated Systems, Inc."));
			break;
		case MM_METHEUS:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Metheus"));
			break;
		case MM_LOGITECH:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Logitech, Inc."));
			break;
		case MM_WINNOV:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Winnov, Inc."));
			break;
		case MM_NCR:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("NCR Corporation"));
			break;
		case MM_EXAN:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("EXAN"));
			break;
		case MM_AST:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("AST Research Inc."));
			break;
		case MM_WILLOWPOND:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Willow Pond Corporation"));
			break;
		case MM_SONICFOUNDRY:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Sonic Foundry"));
			break;
		case MM_VITEC:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Vitec Multimedia"));
			break;
		case MM_MOSCOM:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("MOSCOM Corporation"));
			break;
		case MM_SILICONSOFT:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Silicon Soft, Inc."));
			break;
		case MM_SUPERMAC:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Supermac"));
			break;
		case MM_AUDIOPT:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Audio Processing Technology"));
			break;
		case MM_SPEECHCOMP:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Speech Compression"));
			break;
		case MM_AHEAD:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Ahead, Inc."));
			break;
		case MM_DOLBY:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Dolby Laboratories"));
			break;
		case MM_OKI:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("OKI"));
			break;
		case MM_AURAVISION:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("AuraVision Corporation"));
			break;
		case MM_OLIVETTI:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Ing C. Olivetti & C., S.p.A."));
			break;
		case MM_IOMAGIC:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("I/O Magic Corporation"));
			break;
		case MM_MATSUSHITA:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Matsushita Electric Industrial Co., LTD."));
			break;
		case MM_CONTROLRES:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Control Resources Limited"));
			break;
		case MM_XEBEC:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Xebec Multimedia Solutions Limited"));
			break;
		case MM_NEWMEDIA:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("New Media Corporation"));
			break;
		case MM_NMS:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Natural MicroSystems"));
			break;
		case MM_LYRRUS:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Lyrrus Inc."));
			break;
		case MM_COMPUSIC:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Compusic"));
			break;
		case MM_OPTI:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("OPTI Computers Inc."));
			break;
		case MM_ADLACC:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Adlib Accessories Inc."));
			break;
		case MM_COMPAQ:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Compaq Computer Corp."));
			break;
		case MM_DIALOGIC:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Dialogic Corporation"));
			break;
		case MM_INSOFT:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("InSoft, Inc."));
			break;
		case MM_MPTUS:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("M.P. Technologies, Inc."));
			break;
		case MM_WEITEK:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Weitek"));
			break;
		case MM_LERNOUT_AND_HAUSPIE:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Lernout & Hauspie"));
			break;
		case MM_QCIAR:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Quanta Computer Inc."));
			break;
		case MM_APPLE:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Apple Computer, Inc."));
			break;
		case MM_DIGITAL:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Digital Equipment Corporation"));
			break;
		case MM_MOTU:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Mark of the Unicorn"));
			break;
		case MM_WORKBIT:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Workbit Corporation"));
			break;
		case MM_OSITECH:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Ositech Communications Inc."));
			break;
		case MM_MIRO:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("miro Computer Products AG"));
			break;
		case MM_CIRRUSLOGIC:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Cirrus Logic"));
			break;
		case MM_ISOLUTION:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("ISOLUTION  B.V."));
			break;
		case MM_HORIZONS:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Horizons Technology, Inc"));
			break;
		case MM_CONCEPTS:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Computer Concepts Ltd"));
			break;
		case MM_VTG:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Voice Technologies Group, Inc."));
			break;
		case MM_RADIUS:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Radius"));
			break;
		case MM_ROCKWELL:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Rockwell International"));
			break;
			//case MM_XYZ:
			//	_tcscpy( ptszCompany, _T("Co. XYZ for testing") );
			//	break;
		case MM_OPCODE:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Opcode Systems"));
			break;
		case MM_VOXWARE:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Voxware Inc"));
			break;
		case MM_NORTHERN_TELECOM:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Northern Telecom Limited"));
			break;
		case MM_APICOM:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("APICOM"));
			break;
		case MM_GRANDE:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Grande Software"));
			break;
		case MM_ADDX:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("ADDX"));
			break;
		case MM_WILDCAT:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Wildcat Canyon Software"));
			break;
		case MM_RHETOREX:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Rhetorex Inc"));
			break;
		case MM_BROOKTREE:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Brooktree Corporation"));
			break;
		case MM_ENSONIQ:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("ENSONIQ Corporation"));
			break;
		case MM_FAST:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("///FAST Multimedia AG"));
			break;
		case MM_NVIDIA:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("NVidia Corporation"));
			break;
		case MM_OKSORI:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("OKSORI Co., Ltd."));
			break;
		case MM_DIACOUSTICS:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("DiAcoustics, Inc."));
			break;
		case MM_GULBRANSEN:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Gulbransen, Inc."));
			break;
		case MM_KAY_ELEMETRICS:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Kay Elemetrics, Inc."));
			break;
		case MM_CRYSTAL:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Crystal Semiconductor Corporation"));
			break;
		case MM_SPLASH_STUDIOS:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Splash Studios"));
			break;
		case MM_QUARTERDECK:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Quarterdeck Corporation"));
			break;
		case MM_TDK:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("TDK Corporation"));
			break;
		case MM_DIGITAL_AUDIO_LABS:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Digital Audio Labs, Inc."));
			break;
		case MM_SEERSYS:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Seer Systems, Inc."));
			break;
		case MM_PICTURETEL:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("PictureTel Corporation"));
			break;
		case MM_ATT_MICROELECTRONICS:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("AT&T Microelectronics"));
			break;
		case MM_OSPREY:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Osprey Technologies, Inc."));
			break;
		case MM_MEDIATRIX:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Mediatrix Peripherals"));
			break;
		case MM_SOUNDESIGNS:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("SounDesignS M.C.S. Ltd."));
			break;
		case MM_ALDIGITAL:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("A.L. Digital Ltd."));
			break;
		case MM_SPECTRUM_SIGNAL_PROCESSING:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Spectrum Signal Processing, Inc."));
			break;
		case MM_ECS:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Electronic Courseware Systems, Inc."));
			break;
		case MM_AMD:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("AMD"));
			break;
		case MM_COREDYNAMICS:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Core Dynamics"));
			break;
		case MM_CANAM:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("CANAM Computers"));
			break;
		case MM_SOFTSOUND:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Softsound, Ltd."));
			break;
		case MM_NORRIS:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Norris Communications, Inc."));
			break;
		case MM_DDD:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Danka Data Devices"));
			break;
		case MM_EUPHONICS:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("EuPhonics"));
			break;
		case MM_PRECEPT:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Precept Software, Inc."));
			break;
		case MM_CRYSTAL_NET:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Crystal Net Corporation"));
			break;
		case MM_CHROMATIC:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Chromatic Research, Inc"));
			break;
		case MM_VOICEINFO:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Voice Information Systems, Inc"));
			break;
		case MM_VIENNASYS:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Vienna Systems"));
			break;
		case MM_CONNECTIX:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Connectix Corporation"));
			break;
		case MM_GADGETLABS:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Gadget Labs LLC"));
			break;
		case MM_FRONTIER:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Frontier Design Group LLC"));
			break;
		case MM_VIONA:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Viona Development GmbH"));
			break;
		case MM_CASIO:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Casio Computer Co., LTD"));
			break;
		case MM_DIAMONDMM:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Diamond Multimedia"));
			break;
		case MM_S3:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("S3"));
			break;
		case MM_FRAUNHOFER_IIS:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Fraunhofer"));
			break;
		default:
			_tcscpy_s(ptszCompany, SOUNDCARD_COMPANYNAME_STRLEN, _T("Unknown"));
			break;
	}
}

//***************************************************************************
// Construction/Destruction
//***************************************************************************

CVideoCardInfo::CVideoCardInfo()
{
}

CVideoCardInfo::~CVideoCardInfo()
{
	HWINFO_VIDEOCARD		*pVideoCard = NULL;

	for( int i = 0; i < m_sVideoCardArray.GetCount(); i++ )
	{
		pVideoCard = m_sVideoCardArray.At(i);

		if( pVideoCard )
		{
			delete pVideoCard;
			pVideoCard = NULL;
		}
	}
}

//***************************************************************************
//
BOOL CVideoCardInfo::GetInformation()
{
	HKEY	hSubKey;
	HKEY	hKeyProperty;

	bool	bIsAdd = true;

	TCHAR	tszSubKey[REGISTRY_KEY_STRLEN];

	TCHAR   tszDeviceName[REGISTRY_NAME_STRLEN];
	TCHAR   tszDeviceValue[REGISTRY_VALUE_STRLEN];
	TCHAR   tszVideoName[REGISTRY_NAME_STRLEN];
	TCHAR   tszVideoValue[REGISTRY_VALUE_STRLEN];

	TCHAR   tszValue[REGISTRY_VALUE_STRLEN];
	TCHAR	tszDeviceDesc[REGISTRY_VALUE_STRLEN];
	TCHAR	tszDriverDesc[REGISTRY_VALUE_STRLEN];
	TCHAR	tszDescription[REGISTRY_VALUE_STRLEN];
	TCHAR	tszAdapterString[REGISTRY_VALUE_STRLEN];
	TCHAR	tszChipType[REGISTRY_VALUE_STRLEN];
	TCHAR	tszDacType[REGISTRY_VALUE_STRLEN];
	TCHAR   tszDisplayDrivers[REGISTRY_VALUE_STRLEN];

	TCHAR	*ptszDeviceClsid = NULL;

	DWORD  	dwNameLen = 0;
	DWORD	dwValueLen = 0;
	DWORD   dwValueNumber = 0;
	DWORD	dwValueCount = 0;
	DWORD	dwPropValueNumber = 0;
	DWORD	dwPropValueCount = 0;
	DWORD	dwType = 0;
	DWORD	dwCount = 0;
	DWORD	dwMemorySize = 0;
	long	lRetCode = 0;
	long	lMemorySize = 0;

	FILETIME	MyFileTime;

	HWINFO_VIDEOCARD	*pVideoCard = NULL;

	if( IsWindowVersion(-1, -1, VER_PLATFORM_WIN32_WINDOWS) )
	{
		lRetCode = RegOpenKeyEx(HKEY_LOCAL_MACHINE, WIN_DEVICEMAP_VIDEO_KEY, 0, KEY_READ, &hSubKey);
		if( lRetCode == ERROR_SUCCESS )
		{
			lRetCode = RegQueryInfoKey(hSubKey, NULL, 0, 0, NULL, NULL, NULL, &dwValueNumber, NULL, NULL, NULL, &MyFileTime);
			if( lRetCode == ERROR_SUCCESS )
			{
				while( dwValueNumber > dwValueCount )
				{
					dwNameLen = sizeof(tszDeviceName);
					dwValueLen = sizeof(tszDeviceValue);

					tszDeviceName[0] = '\0';
					tszDeviceValue[0] = '\0';

					lRetCode = RegEnumValue(hSubKey, dwValueCount, tszDeviceName, &dwNameLen, NULL, NULL, (LPBYTE)tszDeviceValue, &dwValueLen);
					if( (lRetCode != ERROR_SUCCESS) || (dwValueLen > REGISTRY_VALUE_STRLEN) )
					{
						tszDeviceName[0] = '\0';
						tszDeviceValue[0] = '\0';
					}

					if( (ptszDeviceClsid = _tcsstr(tszDeviceValue, WIN_CONTROL_VIDEO_REGEX)) != NULL )
					{
						ptszDeviceClsid = ptszDeviceClsid + _tcslen(WIN_CONTROL_VIDEO_REGEX);

						_stprintf_s(tszSubKey, _countof(tszSubKey), _T("%s\\%s"), WIN_CONTROL_VIDEO_KEY, ptszDeviceClsid);

						tszDeviceDesc[0] = '\0';
						tszDriverDesc[0] = '\0';
						tszAdapterString[0] = '\0';
						tszChipType[0] = '\0';
						tszDacType[0] = '\0';
						tszDisplayDrivers[0] = '\0';

						lRetCode = RegOpenKeyEx(HKEY_LOCAL_MACHINE, tszSubKey, 0, KEY_READ, &hKeyProperty);
						if( lRetCode == ERROR_SUCCESS )
						{
							dwPropValueNumber = 0;
							dwPropValueCount = 0;
							if( RegQueryInfoKey(hKeyProperty, NULL, 0, 0, NULL, NULL, NULL, &dwPropValueNumber, NULL, NULL, NULL, &MyFileTime) == ERROR_SUCCESS )
							{
								while( dwPropValueNumber > dwPropValueCount )
								{
									dwNameLen = sizeof(tszVideoName);
									dwValueLen = sizeof(tszVideoValue);

									tszVideoName[0] = '\0';
									tszVideoValue[0] = '\0';

									lRetCode = RegEnumValue(hKeyProperty, dwPropValueCount, tszVideoName, &dwNameLen, NULL, &dwType, (LPBYTE)tszVideoValue, &dwValueLen);
									if( (lRetCode != ERROR_SUCCESS) || (dwValueLen > REGISTRY_VALUE_STRLEN) )
									{
										tszVideoName[0] = '\0';
										tszVideoValue[0] = '\0';
									}
									else
									{
										tszValue[0] = '\0';
										switch( dwType )
										{
											case REG_BINARY:
											{
												if( tszVideoValue && _tcslen(tszVideoValue) > 0 )
												{
													dwCount = 0;
													while( dwCount < dwValueLen )
													{
														_stprintf_s(tszValue, _countof(tszValue), _T("%s%c"), tszValue, *(tszVideoValue + dwCount));
														dwCount++;
													}
												}
												break;
											}
											case REG_SZ:
											{
												_tcscpy_s(tszValue, _countof(tszValue), tszVideoValue);
												break;
											}
											case REG_MULTI_SZ:
											{
												_tcscpy_s(tszValue, _countof(tszValue), tszVideoValue);
												break;
											}
											default:
												break;
										}
									}

									if( _tcscmp(tszVideoName, WIN_VIDEO_DEVICEDESC_NAME) == 0 )
										_tcscpy_s(tszDeviceDesc, _countof(tszDeviceDesc), tszValue);

									if( _tcscmp(tszVideoName, WIN_VIDEO_DRIVERDESC_NAME) == 0 )
										_tcscpy_s(tszDriverDesc, _countof(tszDriverDesc), tszValue);

									if( _tcscmp(tszVideoName, WIN_VIDEO_ADAPTERSTRING_NAME) == 0 )
										_tcscpy_s(tszAdapterString, _countof(tszAdapterString), tszValue);

									if( _tcscmp(tszVideoName, WIN_VIDEO_CHIPTYPE_NAME) == 0 )
										_tcscpy_s(tszChipType, _countof(tszChipType), tszValue);

									if( _tcscmp(tszVideoName, WIN_VIDEO_DACTYPE_NAME) == 0 )
										_tcscpy_s(tszDacType, _countof(tszDacType), tszValue);

									if( _tcscmp(tszVideoName, WIN_VIDEO_INSTALLEDDISPLAYDRIVERS_NAME) == 0 )
										_tcscpy_s(tszDisplayDrivers, _countof(tszDisplayDrivers), tszValue);

									if( _tcscmp(tszVideoName, WIN_VIDEO_MEMORYSIZE_NAME) == 0 )
									{
										dwNameLen = sizeof(tszVideoName);
										dwValueLen = sizeof(dwMemorySize);

										lRetCode = RegQueryValueEx(hKeyProperty, tszVideoName, NULL, &dwNameLen, (LPBYTE)&dwMemorySize, &dwValueLen);
										if( lRetCode == ERROR_SUCCESS )
											lMemorySize = dwMemorySize / (1024 * 1024);
										else lMemorySize = -1;
									}

									dwPropValueCount++;
								}

								if( tszDeviceDesc && _tcslen(tszDeviceDesc) > 0 )
									_tcscpy_s(tszDescription, _countof(tszDescription), tszDeviceDesc);

								if( tszDriverDesc && _tcslen(tszDriverDesc) > 0 )
									_tcscpy_s(tszDescription, _countof(tszDescription), tszDriverDesc);

								if( tszAdapterString && _tcslen(tszAdapterString) > 0 )
								{
									pVideoCard = new HWINFO_VIDEOCARD;

									_tcscpy_s(pVideoCard->m_tszDescription, _countof(pVideoCard->m_tszDescription), tszDescription);
									_tcscpy_s(pVideoCard->m_tszAdapterString, _countof(pVideoCard->m_tszAdapterString), tszAdapterString);
									_tcscpy_s(pVideoCard->m_tszChipType, _countof(pVideoCard->m_tszChipType), tszChipType);
									_tcscpy_s(pVideoCard->m_tszDacType, _countof(pVideoCard->m_tszDacType), tszDacType);
									_tcscpy_s(pVideoCard->m_tszDisplayDrivers, _countof(pVideoCard->m_tszDisplayDrivers), tszDisplayDrivers);

									pVideoCard->m_lMemorySize = lMemorySize;

									bIsAdd = true;
									for( int i = 0; i < m_sVideoCardArray.GetCount(); i++ )
									{
										if( _tcscmp(m_sVideoCardArray.At(i)->m_tszAdapterString, pVideoCard->m_tszAdapterString) == 0 )
										{
											bIsAdd = false;
											break;
										}
									}

									if( bIsAdd )
									{
										m_sVideoCardArray.Add(pVideoCard);
									}
									else
									{
										delete pVideoCard;
										pVideoCard = NULL;
									}
								}
							}
						}

						RegCloseKey(hKeyProperty);
					}

					dwValueCount++;
				}

				RegCloseKey(hSubKey);
			}
		}
	}
	else if( IsWindowVersion(-1, -1, VER_PLATFORM_WIN32_NT) )
	{
		lRetCode = RegOpenKeyEx(HKEY_LOCAL_MACHINE, NT_DEVICEMAP_VIDEO_KEY, 0, KEY_READ, &hSubKey);
		if( lRetCode == ERROR_SUCCESS )
		{
			lRetCode = RegQueryInfoKey(hSubKey, NULL, 0, 0, NULL, NULL, NULL, &dwValueNumber, NULL, NULL, NULL, &MyFileTime);
			if( lRetCode == ERROR_SUCCESS )
			{
				while( dwValueNumber > dwValueCount )
				{
					dwNameLen = sizeof(tszDeviceName);
					dwValueLen = sizeof(tszDeviceValue);

					tszDeviceName[0] = '\0';
					tszDeviceValue[0] = '\0';

					lRetCode = RegEnumValue(hSubKey, dwValueCount, tszDeviceName, &dwNameLen, NULL, NULL, (LPBYTE)tszDeviceValue, &dwValueLen);
					if( (lRetCode != ERROR_SUCCESS) || (dwValueLen > REGISTRY_VALUE_STRLEN) )
					{
						tszDeviceName[0] = '\0';
						tszDeviceValue[0] = '\0';
					}

					if( (ptszDeviceClsid = _tcsstr(tszDeviceValue, NT_CONTROL_VIDEO_REGEX)) != NULL )
					{
						ptszDeviceClsid = ptszDeviceClsid + _tcslen(NT_CONTROL_VIDEO_REGEX);

						_stprintf_s(tszSubKey, _countof(tszSubKey), _T("%s\\%s"), NT_CONTROL_VIDEO_KEY, ptszDeviceClsid);

						tszDeviceDesc[0] = '\0';
						tszDriverDesc[0] = '\0';
						tszAdapterString[0] = '\0';
						tszChipType[0] = '\0';
						tszDacType[0] = '\0';
						tszDisplayDrivers[0] = '\0';

						lRetCode = RegOpenKeyEx(HKEY_LOCAL_MACHINE, tszSubKey, 0, KEY_READ, &hKeyProperty);
						if( lRetCode == ERROR_SUCCESS )
						{
							dwPropValueNumber = 0;
							dwPropValueCount = 0;

							lRetCode = RegQueryInfoKey(hKeyProperty, NULL, 0, 0, NULL, NULL, NULL, &dwPropValueNumber, NULL, NULL, NULL, &MyFileTime);
							if( lRetCode == ERROR_SUCCESS )
							{
								while( dwPropValueNumber > dwPropValueCount )
								{
									dwNameLen = sizeof(tszVideoName);
									dwValueLen = sizeof(tszVideoValue);

									tszVideoName[0] = '\0';
									tszVideoValue[0] = '\0';

									lRetCode = RegEnumValue(hKeyProperty, dwPropValueCount, tszVideoName, &dwNameLen, NULL, &dwType, (LPBYTE)tszVideoValue, &dwValueLen);
									if( (lRetCode != ERROR_SUCCESS) || (dwValueLen > REGISTRY_VALUE_STRLEN) )
									{
										tszVideoName[0] = '\0';
										tszVideoValue[0] = '\0';
									}
									else
									{
										tszValue[0] = '\0';
										switch( dwType )
										{
											case REG_BINARY:
											{
												if( tszVideoValue && _tcslen(tszVideoValue) > 0 )
												{
													dwCount = 0;
													while( dwCount < dwValueLen )
													{
														_stprintf_s(tszValue, _countof(tszValue), _T("%s%c"), tszValue, *(tszVideoValue + dwCount));
														dwCount++;
													}
												}
												break;
											}
											case REG_SZ:
											{
												_tcscpy_s(tszValue, _countof(tszValue), tszVideoValue);
												break;
											}
											case REG_MULTI_SZ:
											{
												_tcscpy_s(tszValue, _countof(tszValue), tszVideoValue);
												break;
											}
											default:
												break;
										}
									}

									if( _tcscmp(tszVideoName, NT_VIDEO_DEVICEDESC_NAME) == 0 )
										_tcscpy_s(tszDeviceDesc, _countof(tszDeviceDesc), tszValue);

									if( _tcscmp(tszVideoName, NT_VIDEO_DRIVERDESC_NAME) == 0 )
										_tcscpy_s(tszDriverDesc, _countof(tszDriverDesc), tszValue);

									if( _tcscmp(tszVideoName, NT_VIDEO_ADAPTERSTRING_NAME) == 0 )
										_tcscpy_s(tszAdapterString, _countof(tszAdapterString), tszValue);

									if( _tcscmp(tszVideoName, NT_VIDEO_CHIPTYPE_NAME) == 0 )
										_tcscpy_s(tszChipType, _countof(tszChipType), tszValue);

									if( _tcscmp(tszVideoName, NT_VIDEO_DACTYPE_NAME) == 0 )
										_tcscpy_s(tszDacType, _countof(tszDacType), tszValue);

									if( _tcscmp(tszVideoName, NT_VIDEO_INSTALLEDDISPLAYDRIVERS_NAME) == 0 )
										_tcscpy_s(tszDisplayDrivers, _countof(tszDisplayDrivers), tszValue);

									if( _tcscmp(tszVideoName, NT_VIDEO_MEMORYSIZE_NAME) == 0 )
									{
										dwNameLen = sizeof(tszVideoName);
										dwValueLen = sizeof(dwMemorySize);

										lRetCode = RegQueryValueEx(hKeyProperty, tszVideoName, NULL, &dwNameLen, (LPBYTE)&dwMemorySize, &dwValueLen);
										if( lRetCode == ERROR_SUCCESS )
											lMemorySize = dwMemorySize / (1024 * 1024);
										else lMemorySize = -1;
									}

									dwPropValueCount++;
								}

								if( tszDeviceDesc && _tcslen(tszDeviceDesc) > 0 )
									_tcscpy_s(tszDescription, _countof(tszDescription), tszDeviceDesc);

								if( tszDriverDesc && _tcslen(tszDriverDesc) > 0 )
									_tcscpy_s(tszDescription, _countof(tszDescription), tszDriverDesc);

								if( tszAdapterString && _tcslen(tszAdapterString) > 0 )
								{
									pVideoCard = new HWINFO_VIDEOCARD;

									_tcscpy_s(pVideoCard->m_tszDescription, _countof(pVideoCard->m_tszDescription), tszDescription);
									_tcscpy_s(pVideoCard->m_tszAdapterString, _countof(pVideoCard->m_tszAdapterString), tszAdapterString);
									_tcscpy_s(pVideoCard->m_tszChipType, _countof(pVideoCard->m_tszChipType), tszChipType);
									_tcscpy_s(pVideoCard->m_tszDacType, _countof(pVideoCard->m_tszDacType), tszDacType);
									_tcscpy_s(pVideoCard->m_tszDisplayDrivers, _countof(pVideoCard->m_tszDisplayDrivers), tszDisplayDrivers);

									pVideoCard->m_lMemorySize = lMemorySize;

									bIsAdd = true;
									for( int i = 0; i < m_sVideoCardArray.GetCount(); i++ )
									{
										if( _tcscmp(m_sVideoCardArray.At(i)->m_tszAdapterString, pVideoCard->m_tszAdapterString) == 0 )
										{
											bIsAdd = false;
											break;
										}
									}

									if( bIsAdd )
									{
										m_sVideoCardArray.Add(pVideoCard);
									}
									else
									{
										delete pVideoCard;
										pVideoCard = NULL;
									}
								}
							}
						}

						RegCloseKey(hKeyProperty);
					}

					dwValueCount++;
				}

				RegCloseKey(hSubKey);
			}
		}
	}

	return true;
}

//***************************************************************************
// Construction/Destruction
//***************************************************************************

CNetworkCardInfo::CNetworkCardInfo()
{
}

CNetworkCardInfo::~CNetworkCardInfo()
{
	HWINFO_NETWORKCARD		*pNetworkCard = NULL;

	for( int i = 0; i < m_sNetworkCardArray.GetCount(); i++ )
	{
		pNetworkCard = m_sNetworkCardArray.At(i);

		if( pNetworkCard )
		{
			delete pNetworkCard;
			pNetworkCard = NULL;
		}
	}
}

//***************************************************************************
//
BOOL CNetworkCardInfo::GetInformation(CWmi &Wmi)
{
	USES_CONVERSION;

	int		nIndex = 0;

	VARIANT		vtProp;
	VARIANT		vtDescription;

	HWINFO_NETWORKCARD	*pNetworkCard = NULL;

	nIndex = Wmi.ExecQuery(_T("Win32_NetworkAdapterConfiguration"));
	if( nIndex < 0 ) return false;

	for( int i = 0; i < nIndex; i++ )
	{
		VariantInit(&vtProp);

		Wmi.GetProperties(i, _T("IPEnabled"), vtProp);

		if( vtProp.vt == VT_BOOL && vtProp.bVal != 0 )
		{
			pNetworkCard = new HWINFO_NETWORKCARD;

			VariantInit(&vtDescription);

			Wmi.GetProperties(i, _T("Description"), vtDescription);

			if( vtDescription.vt == VT_BSTR )
			{
#ifdef _UNICODE	
				_tcscpy_s(pNetworkCard->m_tszDescription, _countof(pNetworkCard->m_tszDescription), OLE2W(vtDescription.bstrVal));
#else
				_tcscpy_s(pNetworkCard->m_tszDescription, _countof(pNetworkCard->m_tszDescription), OLE2A(vtDescription.bstrVal));
#endif
			}

			VariantClear(&vtDescription);

			m_sNetworkCardArray.Add(pNetworkCard);
		}

		VariantClear(&vtProp);
	}

	return true;
}

//***************************************************************************
// Construction/Destruction
//***************************************************************************

CCdromInfo::CCdromInfo()
{
}

CCdromInfo::~CCdromInfo()
{
	HWINFO_CDROM		*pCdrom = NULL;

	for( int i = 0; i < m_sCdromArray.GetCount(); i++ )
	{
		pCdrom = m_sCdromArray.At(i);

		if( pCdrom )
		{
			delete pCdrom;
			pCdrom = NULL;
		}
	}
}

//***************************************************************************
//
BOOL CCdromInfo::GetInformation(CWmi &Wmi)
{
	USES_CONVERSION;

	int		nIndex = 0;

	VARIANT		vtName;
	VARIANT		vtManufacturer;
	VARIANT		vtDescription;

	HWINFO_CDROM	*pCdrom = NULL;

	nIndex = Wmi.ExecQuery(_T("Win32_CDROMDrive"));
	if( nIndex < 0 ) return false;

	for( int i = 0; i < nIndex; i++ )
	{
		pCdrom = new HWINFO_CDROM;

		VariantInit(&vtName);

		Wmi.GetProperties(i, _T("Name"), vtName);

		if( vtName.vt == VT_BSTR )
		{
#ifdef _UNICODE	
			_tcscpy_s(pCdrom->m_tszName, _countof(pCdrom->m_tszName), OLE2W(vtName.bstrVal));
#else
			_tcscpy_s(pCdrom->m_tszName, _countof(pCdrom->m_tszName), OLE2A(vtName.bstrVal));
#endif
		}

		VariantClear(&vtName);

		VariantInit(&vtManufacturer);

		Wmi.GetProperties(i, _T("Manufacturer"), vtManufacturer);

		if( vtManufacturer.vt == VT_BSTR )
		{
#ifdef _UNICODE	
			_tcscpy_s(pCdrom->m_tszManufacturer, _countof(pCdrom->m_tszManufacturer), OLE2W(vtManufacturer.bstrVal));
#else
			_tcscpy_s(pCdrom->m_tszManufacturer, _countof(pCdrom->m_tszManufacturer), OLE2A(vtManufacturer.bstrVal));
#endif
		}

		VariantClear(&vtManufacturer);

		VariantInit(&vtDescription);

		Wmi.GetProperties(i, _T("Description"), vtDescription);

		if( vtDescription.vt == VT_BSTR )
		{
#ifdef _UNICODE	
			_tcscpy_s(pCdrom->m_tszDescription, _countof(pCdrom->m_tszDescription), OLE2W(vtDescription.bstrVal));
#else
			_tcscpy_s(pCdrom->m_tszDescription, _countof(pCdrom->m_tszDescription), OLE2A(vtDescription.bstrVal));
#endif
		}

		VariantClear(&vtDescription);

		m_sCdromArray.Add(pCdrom);
	}

	return true;
}

//***************************************************************************
// Construction/Destruction
//***************************************************************************

CKeyBoardInfo::CKeyBoardInfo()
{
	ZeroMemory(&m_KeyBoard, sizeof(HWINFO_KEYBOARD));
}

CKeyBoardInfo::~CKeyBoardInfo()
{
}

//***************************************************************************
//
BOOL CKeyBoardInfo::GetInformation(CWmi &Wmi)
{
	USES_CONVERSION;

	int		nIndex = 0;

	VARIANT		vtDescription;

	nIndex = Wmi.ExecQuery(_T("Win32_Keyboard"));
	if( nIndex < 0 ) return false;

	VariantInit(&vtDescription);

	Wmi.GetProperties(0, _T("Description"), vtDescription);

	if( vtDescription.vt == VT_BSTR )
	{
#ifdef _UNICODE	
		_tcscpy_s(m_KeyBoard.m_tszDescription, _countof(m_KeyBoard.m_tszDescription), OLE2W(vtDescription.bstrVal));
#else
		_tcscpy_s(m_KeyBoard.m_tszDescription, _countof(m_KeyBoard.m_tszDescription), OLE2A(vtDescription.bstrVal));
#endif
	}

	VariantClear(&vtDescription);

	DetectKbType();

	return true;
}

//***************************************************************************
//
void CKeyBoardInfo::DetectKbType()
{
	int		nRetType = 0;
	TCHAR	tszKeyBoardType[KEYBOARD_TYPE_STRLEN];

	nRetType = ::GetKeyboardType(0);

	switch( nRetType )
	{
		case 1:
			_tcscpy_s(tszKeyBoardType, _countof(tszKeyBoardType), _T("IBM PC/XT or compatible (83-key)"));
			break;
		case 2:
			_tcscpy_s(tszKeyBoardType, _countof(tszKeyBoardType), _T("Olivetti \"ICO\" (102-key)"));
			break;
		case 3:
			_tcscpy_s(tszKeyBoardType, _countof(tszKeyBoardType), _T("IBM PC/AT (84-key) or similar"));
			break;
		case 4:
			_tcscpy_s(tszKeyBoardType, _countof(tszKeyBoardType), _T("IBM enhanced (101- or 102-key)"));
			break;
		case 5:
			_tcscpy_s(tszKeyBoardType, _countof(tszKeyBoardType), _T("Nokia 1050 and similar"));
			break;
		case 6:
			_tcscpy_s(tszKeyBoardType, _countof(tszKeyBoardType), _T("Nokia 9140 and similar"));
			break;
		case 7:
			_tcscpy_s(tszKeyBoardType, _countof(tszKeyBoardType), _T("Japanese"));
			break;
		case 8:
			_tcscpy_s(tszKeyBoardType, _countof(tszKeyBoardType), _T("IBM PC/AT or compatible (101-key)"));
			break;
		default:
			_tcscpy_s(tszKeyBoardType, _countof(tszKeyBoardType), _T("Unknown"));
	}

	_tcscpy_s(m_KeyBoard.m_tszType, _countof(m_KeyBoard.m_tszType), tszKeyBoardType);
}

//***************************************************************************
// Construction/Destruction
//***************************************************************************

CMouseInfo::CMouseInfo()
{
	ZeroMemory(&m_Mouse, sizeof(HWINFO_MOUSE));
}

CMouseInfo::~CMouseInfo()
{
}

//***************************************************************************
//
BOOL CMouseInfo::GetInformation(CWmi &Wmi)
{
	USES_CONVERSION;

	int		nIndex = 0;

	VARIANT		vtName;
	VARIANT		vtManufacturer;
	VARIANT		vtDescription;

	nIndex = Wmi.ExecQuery(_T("Win32_PointingDevice"));
	if( nIndex < 0 ) return false;

	VariantInit(&vtName);

	Wmi.GetProperties(0, _T("Name"), vtName);

	if( vtName.vt == VT_BSTR )
	{
#ifdef _UNICODE	
		_tcscpy_s(m_Mouse.m_tszName, _countof(m_Mouse.m_tszName), OLE2W(vtName.bstrVal));
#else
		_tcscpy_s(m_Mouse.m_tszName, _countof(m_Mouse.m_tszName), OLE2A(vtName.bstrVal));
#endif
	}

	VariantClear(&vtName);

	VariantInit(&vtManufacturer);

	Wmi.GetProperties(0, _T("Manufacturer"), vtManufacturer);

	if( vtManufacturer.vt == VT_BSTR )
	{
#ifdef _UNICODE	
		_tcscpy_s(m_Mouse.m_tszManufacturer, _countof(m_Mouse.m_tszManufacturer), OLE2W(vtManufacturer.bstrVal));
#else
		_tcscpy_s(m_Mouse.m_tszManufacturer, _countof(m_Mouse.m_tszManufacturer), OLE2A(vtManufacturer.bstrVal));
#endif
	}

	VariantClear(&vtManufacturer);

	VariantInit(&vtDescription);

	Wmi.GetProperties(0, _T("Description"), vtDescription);

	if( vtDescription.vt == VT_BSTR )
	{
#ifdef _UNICODE	
		_tcscpy_s(m_Mouse.m_tszDescription, _countof(m_Mouse.m_tszDescription), OLE2W(vtDescription.bstrVal));
#else
		_tcscpy_s(m_Mouse.m_tszDescription, _countof(m_Mouse.m_tszDescription), OLE2A(vtDescription.bstrVal));
#endif
	}

	VariantClear(&vtDescription);

	return true;
}

//***************************************************************************
// Construction/Destruction
//***************************************************************************

CMonitorInfo::CMonitorInfo()
{
}

CMonitorInfo::~CMonitorInfo()
{
	HWINFO_MONITOR	*pMonitor = NULL;

	for( int i = 0; i < m_sMonitorArray.GetCount(); i++ )
	{
		pMonitor = m_sMonitorArray.At(i);

		if( pMonitor )
		{
			delete pMonitor;
			pMonitor = NULL;
		}
	}
}

//***************************************************************************
//
BOOL CMonitorInfo::GetInformation(CWmi &Wmi)
{
	USES_CONVERSION;

	int		nIndex = 0;

	VARIANT		vtManufacturer;
	VARIANT		vtDescription;

	HWINFO_MONITOR	*pMonitor = NULL;

	nIndex = Wmi.ExecQuery(_T("Win32_DesktopMonitor"));
	if( nIndex < 0 ) return false;

	for( int i = 0; i < nIndex; i++ )
	{
		pMonitor = new HWINFO_MONITOR;

		VariantInit(&vtManufacturer);

		Wmi.GetProperties(i, _T("MonitorManufacturer"), vtManufacturer);

		if( vtManufacturer.vt == VT_BSTR )
		{
#ifdef _UNICODE	
			_tcscpy_s(pMonitor->m_tszManufacturer, _countof(pMonitor->m_tszManufacturer), OLE2W(vtManufacturer.bstrVal));
#else
			_tcscpy_s(pMonitor->m_tszManufacturer, _countof(pMonitor->m_tszManufacturer), OLE2A(vtManufacturer.bstrVal));
#endif
		}

		VariantClear(&vtManufacturer);

		VariantInit(&vtDescription);

		Wmi.GetProperties(i, _T("Description"), vtDescription);

		if( vtDescription.vt == VT_BSTR )
		{
#ifdef _UNICODE	
			_tcscpy_s(pMonitor->m_tszDescription, _countof(pMonitor->m_tszDescription), OLE2W(vtDescription.bstrVal));
#else
			_tcscpy_s(pMonitor->m_tszDescription, _countof(pMonitor->m_tszDescription), OLE2A(vtDescription.bstrVal));
#endif
		}

		VariantClear(&vtDescription);

		m_sMonitorArray.Add(pMonitor);
	}

	return true;
}



