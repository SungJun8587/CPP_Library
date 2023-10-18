
//***************************************************************************
// This file contains the implementation of processing for external variables.
// 
//***************************************************************************

#include "pch.h"
#include "BaseGlobal.h"

CMemory* gpMemory = nullptr;
CLogManager* gpLogmanager = nullptr;

class CBaseGlobal
{
public:
	CBaseGlobal()
	{
		gpMemory = new CMemory();
		gpLogmanager = new CLogManager();
	}

	~CBaseGlobal()
	{
		if( gpMemory != nullptr ) delete gpMemory;
		if( gpLogmanager != nullptr ) delete gpLogmanager;
	}
} GCBaseGlobal;