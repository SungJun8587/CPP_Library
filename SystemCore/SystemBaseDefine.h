
//***************************************************************************
// This File include Information about systeminfo define macro or constant type, Type.
// 
//***************************************************************************

#ifndef __SYSTEMBASEDEFINE_H__
#define __SYSTEMBASEDEFINE_H__

//***************************************************************************
//***************************************************************************
//***************************************************************************
//		Define Common Value
//Begin**********************************************************************

// feature #define's
#define FPU_FLAG				0x00000001     // [Bit 00] FPU onboard?                                
#define VME_FLAG				0x00000002     // [Bit 01] Virtual Mode Extension                      
#define DE_FLAG					0x00000004     // [Bit 02] Debugging Extension                         
#define PSE_FLAG				0x00000008     // [Bit 03] Page Size Extension                         
#define TSC_FLAG				0x00000010     // [Bit 04] Time Stamp Counter                          
#define MSR_FLAG				0x00000020     // [Bit 05] Model Specific Registers                    
#define PAE_FLAG				0x00000040     // [Bit 06] Physical Address Extension                  
#define MCE_FLAG				0x00000080     // [Bit 07] Machine Check Exception                     
#define CX8_FLAG				0x00000100     // [Bit 08] CMPXCHG8 instruction supported              
#define APIC_FLAG				0x00000200     // [Bit 09] On-chip APIC hardware supported             
#define RESERVED_10				0x00000400     // [Bit 10] reserved                             
#define SEP_FLAG				0x00000800     // [Bit 11] Fast System Call                            
#define MTRR_FLAG				0x00001000     // [Bit 12] Memory Type Range Registers                 
#define PGE_FLAG				0x00002000     // [Bit 13] Page Global Enable                          
#define MCA_FLAG				0x00004000     // [Bit 14] Machine Check Architecture                  
#define CMOV_FLAG				0x00008000     // [Bit 15] Conditional Move Instruction Supported      
#define PAT_FLAG				0x00010000     // [Bit 16] Page Attribute Table                        
#define PSE36_FLAG				0x00020000     // [Bit 17] 36-bit Page Size Extension                  
#define PSNUM_FLAG				0x00040000     // [Bit 18] Processor Serial Number Present and Enabled 
#define CLFLUSH_FLAG			0x00080000     // [Bit 19] CLFLUSH instruction supported               
#define RESERVED_20				0x00100000     // [Bit 20] reserved                             
#define DTS_FLAG				0x00200000     // [Bit 21] Debug Store                                 
#define ACPI_FLAG				0x00400000     // [Bit 22] Thermal Monitor and Software Clock          
#define MMX_FLAG				0x00800000     // [Bit 23] MMX supported                               
#define FXSR_FLAG				0x01000000     // [Bit 24] Fast Floating Point Save and Restore        
#define SSE_FLAG				0x02000000     // [Bit 25] Streaming SIMD Extensions Supported          
#define SSE2_FLAG				0x04000000     // [Bit 26] Streaming SIMD Extensions 2                 
#define SS_FLAG					0x08000000     // [Bit 27] Self-Snoop                                  
#define RESERVED_28				0x10000000     // [Bit 28] reserved                             
#define TM_FLAG					0x20000000     // [Bit 29] Thermal Monitor Supported                   
#define RESERVED_30				0x40000000     // [Bit 30] reserved                             
#define RESERVED_31				0x80000000     // [Bit 31] reserved                             

// extended feature #define's
#define SSEMMX_FLAG				0x00400000     // [Bit 22] SSE MMX Extensions
#define HAS3DNOW_FLAG			0x40000000     // [Bit 30] 3dNow!
#define EXT3DNOW_FLAG			0x80000000     // [Bit 31] 3dNow! Extensions

// largest extended feature #define's
#define AMD_EXTENDED_FEATURE	0x80000001     // this gets extended processor features for AMD CPUs
#define NAMESTRING_FEATURE		0x80000004     // this is the namestring feature; goes from 0x80000002 to 0x80000004
#define AMD_L1CACHE_FEATURE		0x80000005     // this gets L1 cache info for AMD CPUs
#define AMD_L2CACHE_FEATURE		0x80000006     // this gets L2 cache info for AMD CPUs

#define BRANDTABLESIZE			4

#define VENDOR_INTEL_STR		_T("GenuineIntel")
#define VENDOR_AMD_STR			_T("AuthenticAMD")
#define VENDOR_CYRIX_STR		_T("CyrixInstead")
#define VENDOR_CENTAUR_STR		_T("CentaurHauls")

#define CPU_VENDOR_STRLEN					13
#define CPU_GENNAME_STRLEN					64
#define CPU_DETAILNAME_STRLEN				256

#define	BIOS_MANUFACTURER_STRLEN			128
#define	BIOS_SMVERSION_STRLEN				32
#define	BIOS_VERSION_STRLEN					32
#define	BIOS_IDENTIFICATIONCODE_STRLEN		64
#define	BIOS_SERIALNUMBER_STRLEN			64
#define	BIOS_RELEASEDATE_STRLEN				32

#define MAINBOARD_DESCRIPTION_STRLEN		128
#define MAINBOARD_MANUFACTURER_STRLEN		128
#define MAINBOARD_PRODUCT_STRLEN			64
#define	MAINBOARD_SERIALNUMBER_STRLEN		64

#define	RAM_NAME_STRLEN						128
#define RAM_DEVICELOCATOR_STRLEN			32
#define RAM_BANKLABEL_STRLEN				32
#define RAM_FORMFACTORDESC_STRLEN			32
#define RAM_MEMORYTYPEDESC_STRLEN			32

#define HDDISK_MODEL_STRLEN					32
#define	HDDISK_NAME_STRLEN					32
#define	HDDISK_MANUFACTURER_STRLEN			32
#define	HDDISK_DESCRIPTION_STRLEN			128

#define DRIVE_NAME_STRLEN					16
#define DRIVE_FILESYSTEM_STRLEN				32
	
#define	CDROM_NAME_STRLEN					32
#define	CDROM_MANUFACTURER_STRLEN			32
#define	CDROM_DESCRIPTION_STRLEN			128

#define SOUNDCARD_COMPANYNAME_STRLEN		128
#define SOUNDCARD_PRODUCTNAME_STRLEN		128

#define VIDEOCARD_DESCRIPTION_STRLEN		128
#define VIDEOCARD_ADAPTERSTRING_STRLEN		128
#define VIDEOCARD_CHIPTYPE_STRLEN			128
#define VIDEOCARD_DACTYPE_STRLEN			128
#define VIDEOCARD_DISPLAYDRIVERS_STRLEN		128

#define	NETWORKCARD_DESCRIPTION_STRLEN		128

#define	KEYBOARD_DESCRIPTION_STRLEN			128
#define KEYBOARD_TYPE_STRLEN				128

#define	MOUSE_NAME_STRLEN					128
#define	MOUSE_MANUFACTURER_STRLEN			128
#define	MOUSE_DESCRIPTION_STRLEN			128

#define	MONITOR_MANUFACTURER_STRLEN			128
#define	MONITOR_DESCRIPTION_STRLEN			128

#define OS_DESCRIPTION_STRLEN				256	
#define OS_SERVICEPACK_STRLEN				128
#define OS_PRODUCTTYPE_STRLEN				80

#define IE_BUILD_STRLEN						128
#define IE_VERSION_STRLEN					128
#define DIRECTX_VERSION_STRLEN				128
#define DIRECTX_INSTALLVERSION_STRLEN		128
#define DIRECTX_DESCRIPTION_STRLEN			128

#define JAVAVM_CLASSPATH_STRLEN				512
#define JAVAVM_VERSION_STRLEN				128

#define INSTALL_SWINFO_DISPLAYNAME_STRLEN			260
#define INSTALL_SWINFO_INSTALLSOURCE_STRLEN			260
#define INSTALL_SWINFO_UNINSTALLSTRING_STRLEN		260

#define WIN_DEVICEMAP_VIDEO_KEY						_T("HARDWARE\\DEVICEMAP\\VIDEO")
#define WIN_CONTROL_VIDEO_REGEX						_T("\\Registry\\Machine\\System\\CurrentControlSet\\Control\\Video\\")
#define WIN_CONTROL_VIDEO_KEY						_T("SYSTEM\\CurrentControlSet\\Control\\Video")

#define WIN_VIDEO_DEVICEDESC_NAME				_T("Device Description")
#define WIN_VIDEO_DRIVERDESC_NAME				_T("DriverDesc")
#define WIN_VIDEO_ADAPTERSTRING_NAME			_T("HardwareInformation.AdapterString")
#define WIN_VIDEO_CHIPTYPE_NAME					_T("HardwareInformation.ChipType")
#define WIN_VIDEO_DACTYPE_NAME					_T("HardwareInformation.DacType")
#define WIN_VIDEO_INSTALLEDDISPLAYDRIVERS_NAME	_T("InstalledDisplayDrivers")
#define WIN_VIDEO_MEMORYSIZE_NAME				_T("HardwareInformation.MemorySize")

#define WIN_MICROSOFT_KEY					_T("Software\\Microsoft")

#define WIN_IE_KEY							_T("Internet Explorer")
#define WIN_IE_BUILD_NAME					_T("Build")
#define WIN_IE_VERSION_NAME					_T("Version")

#define WIN_DIRECTX_KEY						_T("DirectX")
#define WIN_DIRECTX_INSTALLVER_NAME			_T("InstalledVersion")
#define WIN_DIRECTX_VERSION_NAME			_T("Version")

#define WIN_JAVA_INSTALLED_COMPONENTS		_T("SOFTWARE\\Microsoft\\Active Setup\\Installed Components\\{08B0E5C0-4FCB-11CF-AAA5-00401C608500}")

#define WIN_MS_JAVAVM_KEY					_T("Software\\Microsoft\\Java VM")
#define WIN_MS_JAVAVM_RUNTIMELIB_NAME		_T("Classpath")
#define WIN_MS_IE_JAVAVM_KEY				_T("Software\\Microsoft\\Internet Explorer\\AdvancedOptions\\JAVA_VM")
#define WIN_MS_JAVAVM_RUNDLL_FILENAME		_T("msjava.dll")

#define WIN_SUN_JAVAVM_KEY										_T("Software\\JavaSoft")
#define WIN_SUN_JAVAVM_PLUGIN_KEY								_T("Software\\JavaSoft\\Java Plug-in")
#define WIN_SUN_JAVAVM_JRE_KEY									_T("Software\\JavaSoft\\Java Runtime Environment")
#define WIN_SUN_JAVAVM_RUNTIMELIB_NAME							_T("RuntimeLib")
#define WIN_SUN_JAVAVM_JAVAHOME_NAME							_T("JavaHome")
#define WIN_SUN_JAVAVM_INSTALL_DIRECTORY						_T("C:\\Program Files\\Java\\")

#define WIN_UNINSTALL_MSJVM_UNINSTALLER_CLASSNAME				_T("Microsoft VM uninstall")
#define WIN_UNINSTALL_SUNJVM_MSI_EXEC							_T("msiexec.exe /x {3248F0A8-6813-11D6-A77B-00B0D0%s%s%s%s0}")
#define WIN_UNINSTALL_SUNJVM_MSI_WINDOWSINSTALLER_CLASSNAME		_T("Windows Installer")

#define WIN_SOFTWARE_UNINSTALL_KEY								_T("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall")				

#define WIN_SOFTWARE_UNINSTALL_DISPLAYNAME_NAME					_T("DisplayName")				
#define WIN_SOFTWARE_UNINSTALL_INSTALLSOURCE_NAME				_T("InstallSource")				
#define WIN_SOFTWARE_UNINSTALL_UNINSTALLSTRING_NAME				_T("UninstallString")				

#define NT_DEVICEMAP_VIDEO_KEY						_T("HARDWARE\\DEVICEMAP\\VIDEO")
#define NT_CONTROL_VIDEO_REGEX						_T("\\Registry\\Machine\\System\\CurrentControlSet\\Control\\Video\\")
#define NT_CONTROL_VIDEO_KEY						_T("SYSTEM\\CurrentControlSet\\Control\\Video")

#define NT_VIDEO_DEVICEDESC_NAME				_T("Device Description")
#define NT_VIDEO_DRIVERDESC_NAME				_T("DriverDesc")
#define NT_VIDEO_ADAPTERSTRING_NAME				_T("HardwareInformation.AdapterString")
#define NT_VIDEO_CHIPTYPE_NAME					_T("HardwareInformation.ChipType")
#define NT_VIDEO_DACTYPE_NAME					_T("HardwareInformation.DacType")
#define NT_VIDEO_INSTALLEDDISPLAYDRIVERS_NAME	_T("InstalledDisplayDrivers")
#define NT_VIDEO_MEMORYSIZE_NAME				_T("HardwareInformation.MemorySize")

#define NT_MICROSOFT_KEY					_T("Software\\Microsoft")

#define NT_IE_KEY							_T("Internet Explorer")
#define NT_IE_BUILD_NAME					_T("Build")
#define NT_IE_VERSION_NAME					_T("Version")

#define NT_DIRECTX_KEY						_T("DirectX")
#define NT_DIRECTX_INSTALLVER_NAME			_T("InstalledVersion")
#define NT_DIRECTX_VERSION_NAME				_T("Version")

#define NT_MS_JAVAVM_KEY					_T("Software\\Microsoft\\Java VM")
#define NT_MS_JAVAVM_RUNTIMELIB_NAME		_T("Classpath")
#define NT_MS_IE_JAVAVM_KEY					_T("Software\\Microsoft\\Internet Explorer\\AdvancedOptions\\JAVA_VM")
#define NT_MS_JAVAVM_RUNDLL_FILENAME		_T("msjava.dll")

#define NT_SUN_JAVAVM_KEY					_T("Software\\JavaSoft")
#define NT_SUN_JAVAVM_PLUGIN_KEY			_T("Software\\JavaSoft\\Java Plug-in")
#define NT_SUN_JAVAVM_JRE_KEY				_T("Software\\JavaSoft\\Java Runtime Environment")
#define NT_SUN_JAVAVM_JAVAHOME_NAME			_T("JavaHome")
#define NT_SUN_JAVAVM_RUNTIMELIB_NAME		_T("RuntimeLib")
#define NT_SUN_JAVAVM_INSTALL_DIRECTORY		_T("C:\\Program Files\\Java\\")

#define NT_JAVA_INSTALLED_COMPONENTS							_T("SOFTWARE\\Microsoft\\Active Setup\\Installed Components\\{08B0E5C0-4FCB-11CF-AAA5-00401C608500}")

#define NT_UNINSTALL_SUNJVM_MSI_EXEC							_T("msiexec.exe /passive /norestart /x {3248F0A8-6813-11D6-A77B-00B0D0%s%s%s%s0}")
#define NT_UNINSTALL_SUNJVM_MSI_WINDOWSINSTALLER_CLASSNAME		_T("")

#define UNINSTALL_MSJVM_TOOL_EXEC								_T("%s\\unmsjvm.exe")
#define UNINSTALL_MSJVM_DEL_DIRECTORY							_T("Java")
#define UNINSTALL_MSJVM_DEL_FILE_JVIEW							_T("jview.exe")
#define UNINSTALL_MSJVM_DEL_FILE_WJVIEW							_T("wjview.exe")
#define UNINSTALL_MSJVM_TOOL_CLASSNAME							_T("Microsoft Java Virtual Machine Removal Tool")

#define INSTALL_SUNJVM_MSI_EXEC									_T("%s\\jre-1_5_0_06.exe")
#define INSTALL_SUNJVM_MSI_WINDOWSINSTALLER_CLASSNAME			_T("Windows Installer")
#define INSTALL_SUNJVM_MSI_NOCLOSE_CLASSNAME					_T("MsiDialogNoCloseClass")
#define INSTALL_SUNJVM_MSI_CLOSE_CLASSNAME						_T("MsiDialogCloseClass")

#define UNINSTALL_SUNJVM_MSI_WINDOWNAME_STR						_T("J2SE Runtime Environment %s.%s Update %d")

//End************************************************************************
//***************************************************************************
//***************************************************************************

#if defined(_M_IX86)
	#include <CpuInfo86.h>
#else
	#include <CpuInfo64.h>
#endif

#endif // ndef __BASESYSTEMINFO_H__