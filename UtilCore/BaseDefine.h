
//***************************************************************************
// This File include Information about define macro or constant type, Type.
// 
//***************************************************************************

#ifndef __BASEDEFINE_H__
#define __BASEDEFINE_H__

#pragma once

//***************************************************************************
//***************************************************************************
//***************************************************************************
//      Exception Error Code
//Begin**********************************************************************

#define STATUS_INSUFFICIENT_MEM			0xE0000001
#define STATUS_PTR_INDEX_INVALID		0xE0000002	

//End************************************************************************
//***************************************************************************
//***************************************************************************


//***************************************************************************
//***************************************************************************
//***************************************************************************
//		Define Common Value
//Begin**********************************************************************

#define MAX_BUFFER_SIZE								1024
#define MAX_PACKET_SIZE								1024

#define NUMERIC_STRING_LEN							20

#define REQUEST_URL_STRLEN							1024
#define REDIRECT_MOVE_URL_STRLEN					512
#define SITE_DOMAIN_STRLEN							32
#define DEFAULT_SITE_URL_STRLEN						128
#define QUERYSTRING_STRLEN							512
#define COOKIE_SIZE_STRLEN							4096
#define COOKIE_VALUE_STRLEN							32
#define COOKIE_EXPIRES_DATETIME_STRLEN				32

// FullFilePath(Drive:\\Folder\\FileName.Ext) : MAX_PATH = 260 
// MAX_DRIVE = 4
// MAX_DIR = 255
// MAX_FILENAMEEXT = 255
// MAX_FILENAME = 251
// MAX_FILEEXT = 4
#define FULLPATH_STRLEN						MAX_PATH
#define DRIVE_STRLEN						3 + 1
#define DIRECTORY_STRLEN					255	+ 1
#define FILENAMEEXT_STRLEN					255	+ 1
#define FILENAME_STRLEN						251	+ 1
#define FILEEXT_STRLEN						4 + 1

// RegSubKey : MAX_PATH = 260
// RegName : MAX_PATH = 260
// RegValue : Realloc 
#define REGISTRY_KEY_STRLEN							260
#define REGISTRY_NAME_STRLEN						260
#define REGISTRY_VALUE_STRLEN						1024

#define STD_DATETIME_STRLEN							32	// 'Thu, 2 Aug 2008 20:47:11 GMT'
#define DATETIME_STRLEN								24	// '2009-07-30 11:00:00'
#define DATE_STRLEN									11	// '2009-07-30'
#define TIME_STRLEN									9	// '11:00:00'
#define MONTH_ENAME_STRLEN							16  // 'January'
#define WEEKDAY_ENAME_STRLEN						16  // 'Sunday'

#define TOKENIZE_LEN								256
#define FILEATT_DATETIME_STRLEN						32

#define MD5_HASHKEY_STRLEN							32

#define HOSTNAME_STRLEN								128
#define SERVICE_TYPE_STRLEN							4
#define PROTOCOL_STRLEN								20
#define PORT_STRLEN									5
#define IP_STRLEN									22								// INET_ADDRSTRLEN = 22, 'xxx.xxx.xxx.xxx'
#define IP_PORT_STRLEN								IP_STRLEN + PORT_STRLEN + 1		// 'xxx.xxx.xxx.xxx:zzzz'
#define IP6_STRLEN									65								// INET6_ADDRSTRLEN = 65, 'xxxx.xxxx.xxxx.xxxx'
#define IP6_PORT_STRLEN								IP6_STRLEN + PORT_STRLEN + 1	// 'xxxx.xxxx.xxxx.xxxx:zzzz'

#define PC_NAME_STRLEN								128

#define USER_ID_STRLEN								100
#define USER_EMAIL_STRLEN							50
#define USER_NAME_STRLEN							100

#define DATABASE_DSN_STRLEN							256
#define PROVIDER_SOURCE_NAME_STRLEN					32
#define DATABASE_DSN_DRIVER_STRLEN					32
#define DATABASE_SERVER_NAME_STRLEN					32
#define DATABASE_NAME_STRLEN						32
#define DATABASE_DSN_USER_ID_STRLEN					32
#define DATABASE_DSN_USER_PASSWORD_STRLEN			32

#define DATABASE_CHARACTERSET_STRLEN				50
#define DATABASE_TABLE_NAME_STRLEN					128
#define DATABASE_COLUMN_NAME_STRLEN					128
#define DATABASE_OBJECT_NAME_STRLEN					128
#define DATABASE_OBJECT_TYPE_STRLEN					3
#define DATABASE_OBJECT_TYPE_DESC_STRLEN			61
#define DATABASE_DATATYPEDESC_STRLEN				128
#define DATABASE_BASE_STRLEN						260
#define DATABASE_SCHEDULE_TIME_STRLEN				8

#define DATABASE_DEFAULT_LOGIN_TIMEOUT				5	// Login Timeout(sec)
#define DATABASE_DEFAULT_CONNECTION_TIMEOUT			10	// Any Request Timeout(sec)
#define DATABASE_DEFAULT_QUERY_TIMEOUT				5	// Query Timeout(sec)

#define DATABASE_BUFFER_SIZE						2048
#define DATABASE_ERRORMSG_STRLEN					4096	

#define DATABASE_VARCHAR_MAX						8000
#define DATABASE_WVARCHAR_MAX						4000
#define DATABASE_BINARY_MAX							8000

#define MYSQL_MAX_MESSAGE_LENGTH					512
#define MYSQL_DEFAULT_CONNECTION_TIMEOUT			2
#define MYSQL_DEFAULT_QUERY_READ_TIMEOUT			2
#define MYSQL_DEFAULT_QUERY_WRITE_TIMEOUT			2

#define DATABASE_OBJECT_CONTENTTEXT_STRLEN			10000

#define	SERVICE_INSTALL						_T("install")
#define	SERVICE_UNINSTALL					_T("uninstall")
#define	SERVICE_CTRL_START					_T("start")
#define	SERVICE_CTRL_STOP					_T("stop")
#define SERVICE_DEBUG						_T("console")
#define DEFAULT_DEP_STR						_T("\0\0")
#define REPORT_TIMEOUT						0

#define DEADLOCK_STAMP_FREQ				(5*1000)			// Dead Lock 인지 체크 주기 (5 sec.)
#define DEADLOCK_STAMP_CNT				10					// DEADLOCK_STAMP_FREQ(5초)주기로 10번 체크하여 Count가 0이 되면 서버 강제종료(30 sec.)

#define _CONSOLE_LOG
//#define _FILE_LOG

//End************************************************************************
//***************************************************************************
//***************************************************************************


//***************************************************************************
//      Define Value Of WebCommon 
//Begin**********************************************************************

#define HTTP_TOTAL_URL_STRLEN						5257	// 'http://www.naver.com/community/bbs/list.asp?searchtype=f_subject&searchstring=apple'
#define HTTP_PROTOCOL_HOSTNAME_STRLEN				137		// 'http://www.naver.com'	
#define HTTP_URLPATH_QUERYSTRING_STRLEN				5120	// '/community/bbs/list.asp?searchtype=f_subject&searchstring=apple'
#define HTTP_PROTOCOL_STRLEN						9		// 'https://'
#define HTTP_HOSTNAME_STRLEN						128		// 'www.naver.com'
#define HTTP_URLPATH_STRLEN							2048	// '/community/bbs/list.asp'
#define HTTP_QUERYSTRING_STRLEN						4096	// 'searchtype=f_subject&searchstring=apple'
#define HTTP_STATUS_STRLEN							32		// '200 OK', '400 Bad Request'

#define	HTTP_HEADER_TOTAL_SIZE						16384	// HTTP Header Total Size
#define	HTTP_HEADER_BOUNDARY_STRLEN					256		// '---------------------------48363CDC004391'
#define HTTP_HEADER_METHOD_STRLEN					8		// 'GET', 'POST', 'HEAD', 'TRACE', 'PUT', 'DELETE', 'OPTIONS', 'CONNECT'						
#define HTTP_HEADER_HTTPVERSION_STRLEN				9		// 'HTTP/1.0', 'HTTP/1.1'
#define HTTP_HEADER_CONNECTION_STRLEN				16		// 'keep-alive', 'close'
#define HTTP_HEADER_ACCEPT_STRLEN					256		// 'text/html, image/gif, image/x-xbitmap, image/jpeg, image/pjpeg, application/vnd.ms-excel, application/msword, application/vnd.ms-powerpoint, */*'
#define HTTP_HEADER_ACCEPTLANGUAGE_STRLEN			32		// 'de,en;q=0.7,en-us;q=0.3'
#define HTTP_HEADER_CONTENTTYPE_STRLEN				128		// 'application/x-www-form-urlencoded'
#define HTTP_HEADER_CONTENTLENGTH_STRLEN			16		// '32'
#define HTTP_HEADER_CHARSET_STRLEN					16		// 'utf-8', 'euc-kr'
#define HTTP_HEADER_ACCEPTENCODING_STRLEN			32		// 'gzip, deflate'
#define HTTP_HEADER_ACCEPTCHARSET_STRLEN			32		// 'iso-8859-5', 'ks_c_5601-1987', 'utf-8'
#define HTTP_HEADER_CACHECONTROL_STRLEN				32		// 'private', 'no-cache', 'no-store'
#define HTTP_HEADER_PRAGMA_STRLEN					20		// 'no-cache'
#define HTTP_HEADER_USERAGENT_STRLEN				128		// 'Mozilla/4.0 (compatible; MSIE 7.0; Windows NT 5.1)'
#define HTTP_HEADER_COOKIE_STRLEN					4096	// 'key=value'	
#define HTTP_HEADER_STATUSCODE_STRLEN				16		// '200'
#define HTTP_HEADER_STATUSMSG_STRLEN				32		// 'OK'
#define HTTP_HEADER_SERVER_STRLEN					64		// 'Microsoft-IIS/5.0'
#define HTTP_FORMNAME_STRLEN						32

#define HTTP_REQ_HEADER_HOST							_T("Host:")					// Host: www.naver.com'
#define HTTP_REQ_HEADER_CONNECTION						_T("Connection:")			// Connection: close	
#define HTTP_REQ_HEADER_ACCEPT							_T("Accept:")				// Accept: */*
#define HTTP_REQ_HEADER_ACCEPTLANGUAGE					_T("Accept-Language:")		// Accept-Language: de,en;q=0.7,en-us;q=0.3
#define HTTP_REQ_HEADER_CONTENTTYPE						_T("Content-Type:")			// Content-Type: application/x-www-form-urlencoded
#define HTTP_REQ_HEADER_CONTENTLENGTH					_T("Content-Length:")		// Content-Length: 0
#define	HTTP_REQ_HEADER_CHARSET							_T("Charset=")				// charset=utf-8
#define HTTP_REQ_HEADER_ACCEPTENCODING					_T("Accept-Encoding:")		// Accept-Encoding: gzip
#define HTTP_REQ_HEADER_ACCEPTCHARSET					_T("Accept-Charset:")		// Accept-Charset: ISO-8859-1,UTF-8;q=0.7,*;q=0.7
#define HTTP_REQ_HEADER_CACHECONTROL					_T("Cache-Control:")		// Cache-Control: private 
#define HTTP_REQ_HEADER_PRAGMA							_T("Pragma:")				// Pragma: no-cache
#define HTTP_REQ_HEADER_REFERER							_T("Referer:")				// www.test.com
#define HTTP_REQ_HEADER_USERAGENT						_T("User-Agent:")			// User-Agent: Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.0)
#define HTTP_REQ_HEADER_COOKIES							_T("Cookie:")				// Cookie: key=value

#define HTTP_RES_HEADER_SERVER							_T("Server:")				// Server: Microsoft-IIS/5.0
#define HTTP_RES_HEADER_DATE							_T("Date:")					// Date: Fri, 31 Jul 2009 09:19:51 GMT
#define HTTP_RES_HEADER_CONNECTION						_T("Connection:")			// Connection: close
#define HTTP_RES_HEADER_PRAGMA							_T("Pragma:")				// pragma: no-cache
#define HTTP_RES_HEADER_CACHECONTROL					_T("Cache-Control:")		// cache-control: private
#define HTTP_RES_HEADER_CONTENTTYPE						_T("Content-Type:")			// Content-Type: text/html; Charset=ks_c_5601-1987	
#define HTTP_RES_HEADER_CONTENTLENGTH					_T("Content-Length:")		// Content-Length: 32282
#define	HTTP_RES_HEADER_CHARSET							_T("Charset=")				// charset=utf-8
#define HTTP_RES_HEADER_EXPIRES							_T("Expires:")				// Expires: Fri, 31 Jul 2009 09:19:51 GMT
#define HTTP_RES_HEADER_SETCOOKIE						_T("Set-Cookie:")			// Set-Cookie: ASPSESSIONIDAADBRSRS=OGDPADCDLHNGFLFKHDABOBBN; path=/	
#define HTTP_RES_HEADER_LASTMODIFIED					_T("Last-Modified:")		// Last-Modified: Fri, 31 Jul 2009 07:00:10 GMT
#define HTTP_RES_TEXT_CONTENTTYPE						_T("text/html;text/xml")
#define HTTP_FORM_CONTENT_DISPOSITION					_T("Content-Disposition: form-data; name=")

#define HTTP_HEADER_BOUNDARY							_T("boundary=")

#define COOKIE_PASSOR								_T(';')
#define COOKIE_PASSOR_STR							_T(";")
#define QUERY_STRING_TOKEN							_T("&")
#define QUERY_CHAR_TOKEN							_T('&')
#define URL_QUERYSTRING_STRING_TOKEN				_T("?")
#define URL_QUERYSTRING_CHAR_TOKEN					_T('?')
#define NULL_TEMINATOR_CHAR_TOKEN					_T('\0')
#define FORM_DATA_HEADER_STRING						_T("application/x-www-form-urlencoded")
#define MULTIPART_FORM_DATA_HEADER_STRING			_T("multipart/form-data")
#define CRLF_STRING									_T("\r\n")
//End************************************************************************
//***************************************************************************
//***************************************************************************

#endif // ndef __BASEDEFINE_H__

