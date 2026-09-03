
//***************************************************************************
// This File include Information about overriding the db data type.
// 
//***************************************************************************

#ifndef UC_DBREDEFINEDATATYPE_H
#define UC_DBREDEFINEDATATYPE_H

#include <BaseRedefineDataType.h>
#include <BaseMacro.h>

NAMESPACE_BEGIN(DBModel)

//***************************************************************************
// 프로젝트 내 주요 클래스 스마트 포인터 정의
//***************************************************************************
USING_SHARED_PTR(Column);
USING_SHARED_PTR(Constraint);
USING_SHARED_PTR(IdentityColumn);
USING_SHARED_PTR(IndexColumn);
USING_SHARED_PTR(Index);
USING_SHARED_PTR(IndexOption);
USING_SHARED_PTR(ForeignKey);
USING_SHARED_PTR(DefaultConstraint);
USING_SHARED_PTR(CheckConstraint);
USING_SHARED_PTR(Table);
USING_SHARED_PTR(Trigger);
USING_SHARED_PTR(ProcParam);
USING_SHARED_PTR(Procedure);
USING_SHARED_PTR(FuncParam);
USING_SHARED_PTR(Function);

NAMESPACE_END

#endif // ndef UC_DBREDEFINEDATATYPE_H