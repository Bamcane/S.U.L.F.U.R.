/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <new>

#include <base/math.h>
#include <base/system.h>

#include <engine/shared/protocol.h>
#include <engine/storage.h>

#include "config.h"
#include "console.h"
#include "linereader.h"

// todo: rework this

const char *CConsole::CResult::GetString(unsigned Index)
{
	if(Index >= m_NumArgs)
		return "";
	return m_apArgs[Index];
}

int CConsole::CResult::GetInteger(unsigned Index)
{
	if(Index >= m_NumArgs)
		return 0;
	return str_toint(m_apArgs[Index]);
}

float CConsole::CResult::GetFloat(unsigned Index)
{
	if(Index >= m_NumArgs)
		return 0.0f;
	return str_tofloat(m_apArgs[Index]);
}

const IConsole::CCommandInfo *CConsole::CCommand::NextCommandInfo(int AccessLevel, int FlagMask) const
{
	const CCommand *pInfo = m_pNext;
	while(pInfo)
	{
		if(pInfo->m_Flags & FlagMask && pInfo->m_AccessLevel >= AccessLevel)
			break;
		pInfo = pInfo->m_pNext;
	}
	return pInfo;
}

const IConsole::CCommandInfo *CConsole::FirstCommandInfo(int AccessLevel, int FlagMask) const
{
	for(const CCommand *pCommand = m_pFirstCommand; pCommand; pCommand = pCommand->m_pNext)
	{
		if(pCommand->m_Flags & FlagMask && pCommand->GetAccessLevel() >= AccessLevel)
			return pCommand;
	}

	return 0;
}

// the maximum number of tokens occurs in a string of length CONSOLE_MAX_STR_LENGTH with tokens size 1 separated by single spaces

int CConsole::ParseStart(CResult *pResult, const char *pString, int Length)
{
	char *pStr;
	int Len = sizeof(pResult->m_aStringStorage);
	if(Length < Len)
		Len = Length;

	str_copy(pResult->m_aStringStorage, pString, Len);
	pStr = pResult->m_aStringStorage;

	// get command
	pStr = str_skip_whitespaces(pStr);
	pResult->m_pCommand = pStr;
	pStr = str_skip_to_whitespace(pStr);

	if(*pStr)
	{
		pStr[0] = 0;
		pStr++;
	}

	pResult->m_pArgsStart = pStr;
	return 0;
}

bool CConsole::ArgStringIsValid(const char *pFormat)
{
	char Command = *pFormat;
	bool Valid = true;
	bool Last = false;

	while(Valid)
	{
		if(!Command)
			break;

		if(Last && *pFormat)
			return false;

		if(Command == '?')
		{
			if(!pFormat[1])
				return false;
		}
		else
		{
			if(Command == 'i' || Command == 'f' || Command == 's')
				;
			else if(Command == 'r')
				Last = true;
			else
				return false;
		}

		Valid = !NextParam(&Command, pFormat);
	}

	return Valid;
}

int CConsole::ParseArgs(CResult *pResult, const char *pFormat)
{
	char Command = *pFormat;
	char *pStr;
	int Optional = 0;
	int Error = 0;

	pStr = pResult->m_pArgsStart;

	while(!Error)
	{
		if(!Command)
			break;

		if(Command == '?')
			Optional = 1;
		else
		{
			pStr = str_skip_whitespaces(pStr);

			if(!(*pStr)) // error, non optional command needs value
			{
				if(!Optional)
				{
					Error = 1;
				}
				break;
			}

			// add token
			if(*pStr == '"')
			{
				char *pDst;
				pStr++;
				pResult->AddArgument(pStr);

				pDst = pStr; // we might have to process escape data
				while(1)
				{
					if(pStr[0] == '"')
						break;
					else if(pStr[0] == '\\')
					{
						if(pStr[1] == '\\')
							pStr++; // skip due to escape
						else if(pStr[1] == '"')
							pStr++; // skip due to escape
					}
					else if(pStr[0] == 0)
						return 1; // return error

					*pDst = *pStr;
					pDst++;
					pStr++;
				}

				// write null termination
				*pDst = 0;

				pStr++;
			}
			else
			{
				pResult->AddArgument(pStr);

				if(Command == 'r') // rest of the string
				{
					str_utf8_trim_whitespaces_right(pStr);
					break;
				}
				else if(Command == 'i') // validate int
					pStr = str_skip_to_whitespace(pStr);
				else if(Command == 'f') // validate float
					pStr = str_skip_to_whitespace(pStr);
				else if(Command == 's') // validate string
					pStr = str_skip_to_whitespace(pStr);

				if(pStr[0] != 0) // check for end of string
				{
					pStr[0] = 0;
					pStr++;
				}
			}
		}

		// fetch next command
		Error = NextParam(&Command, pFormat);
	}

	return Error;
}

bool CConsole::NextParam(char *pNext, const char *&pFormat)
{
	if(*pFormat)
	{
		pFormat++;

		if(*pFormat == '[')
		{
			// skip bracket contents
			pFormat += str_span(pFormat, "]");
			if(!*pFormat)
				return true;

			// skip ']'
			pFormat++;
		}

		// skip space if there is one
		pFormat = str_skip_whitespaces_const(pFormat);
	}
	*pNext = *pFormat;
	return false;
}

int CConsole::ParseCommandArgs(const char *pArgs, const char *pFormat, FCommandCallback pfnCallback, void *pContext)
{
	CResult Result;
	str_copy(Result.m_aStringStorage, pArgs, sizeof(Result.m_aStringStorage));
	Result.m_pArgsStart = Result.m_aStringStorage;

	int Error = ParseArgs(&Result, pFormat);
	if(Error)
		return Error;

	if(pfnCallback)
		pfnCallback(&Result, pContext);

	return 0;
}

int CConsole::RegisterPrintCallback(int OutputLevel, FPrintCallback pfnPrintCallback, void *pUserData)
{
	if(m_NumPrintCB == MAX_PRINT_CB)
		return -1;

	m_aPrintCB[m_NumPrintCB].m_OutputLevel = clamp(OutputLevel, (int) (OUTPUT_LEVEL_STANDARD), (int) (OUTPUT_LEVEL_DEBUG));
	m_aPrintCB[m_NumPrintCB].m_pfnPrintCallback = pfnPrintCallback;
	m_aPrintCB[m_NumPrintCB].m_pPrintCallbackUserdata = pUserData;
	return m_NumPrintCB++;
}

void CConsole::SetPrintOutputLevel(int Index, int OutputLevel)
{
	if(Index >= 0 && Index < MAX_PRINT_CB)
		m_aPrintCB[Index].m_OutputLevel = clamp(OutputLevel, (int) (OUTPUT_LEVEL_STANDARD), (int) (OUTPUT_LEVEL_DEBUG));
}

void CConsole::Print(int Level, const char *pFrom, const char *pStr, bool Highlighted)
{
	char aTimeBuf[80];
	str_timestamp_format(aTimeBuf, sizeof(aTimeBuf), FORMAT_TIME);

	char aBuf[1024];
	str_format(aBuf, sizeof(aBuf), "[%s][%s]: %s", aTimeBuf, pFrom, pStr);
	dbg_msg(pFrom, "%s", pStr);
	for(int i = 0; i < m_NumPrintCB; ++i)
	{
		if(Level <= m_aPrintCB[i].m_OutputLevel && m_aPrintCB[i].m_pfnPrintCallback)
		{
			m_aPrintCB[i].m_pfnPrintCallback(aBuf, m_aPrintCB[i].m_pPrintCallbackUserdata, Highlighted);
		}
	}
}

bool CConsole::LineIsValid(const char *pStr)
{
	if(!pStr)
		return false;

	// Comments and empty lines are valid commands
	if(*pStr == '#' || *pStr == '\0')
		return true;

	do
	{
		CResult Result;
		const char *pEnd = pStr;
		const char *pNextPart = 0;
		int InString = 0;

		while(*pEnd)
		{
			if(*pEnd == '"')
				InString ^= 1;
			else if(*pEnd == '\\') // escape sequences
			{
				if(pEnd[1] == '"')
					pEnd++;
			}
			else if(!InString)
			{
				if(*pEnd == ';') // command separator
				{
					pNextPart = pEnd + 1;
					break;
				}
				else if(*pEnd == '#') // comment, no need to do anything more
					break;
			}

			pEnd++;
		}

		if(ParseStart(&Result, pStr, (pEnd - pStr) + 1) != 0)
			return false;

		CCommand *pCommand = FindCommand(Result.m_pCommand, m_FlagMask);
		if(!pCommand || ParseArgs(&Result, pCommand->m_pParams))
			return false;

		pStr = pNextPart;
	} while(pStr && *pStr);

	return true;
}

void CConsole::ExecuteLineStroked(int Stroke, const char *pStr)
{
	while(pStr && *pStr)
	{
		CResult Result;
		const char *pEnd = pStr;
		const char *pNextPart = 0;
		int InString = 0;

		while(*pEnd)
		{
			if(*pEnd == '"')
				InString ^= 1;
			else if(*pEnd == '\\') // escape sequences
			{
				if(pEnd[1] == '"')
					pEnd++;
			}
			else if(!InString)
			{
				if(*pEnd == ';') // command separator
				{
					pNextPart = pEnd + 1;
					break;
				}
				else if(*pEnd == '#') // comment, no need to do anything more
					break;
			}

			pEnd++;
		}

		if(ParseStart(&Result, pStr, (pEnd - pStr) + 1) != 0)
			return;

		if(!*Result.m_pCommand)
			return;

		CCommand *pCommand = FindCommand(Result.m_pCommand, m_FlagMask);

		if(pCommand)
		{
			if(pCommand->GetAccessLevel() >= m_AccessLevel)
			{
				int IsStrokeCommand = 0;
				if(Result.m_pCommand[0] == '+')
				{
					// insert the stroke direction token
					Result.AddArgument(m_apStrokeStr[Stroke]);
					IsStrokeCommand = 1;
				}

				if(Stroke || IsStrokeCommand)
				{
					if(ParseArgs(&Result, pCommand->m_pParams))
					{
						char aBuf[256];
						str_format(aBuf, sizeof(aBuf), "Invalid arguments... Usage: %s %s", pCommand->m_pName, pCommand->m_pParams);
						Print(OUTPUT_LEVEL_STANDARD, "console", aBuf);
					}
					else if(m_StoreCommands && pCommand->m_Flags & CFGFLAG_STORE)
					{
						m_ExecutionQueue.AddEntry();
						m_ExecutionQueue.m_pLast->m_pCommand = pCommand;
						m_ExecutionQueue.m_pLast->m_Result = Result;
					}
					else
						pCommand->m_pfnCallback(&Result, pCommand->m_pUserData);
				}
			}
			else if(Stroke)
			{
				char aBuf[256];
				str_format(aBuf, sizeof(aBuf), "Access for command %s denied.", Result.m_pCommand);
				Print(OUTPUT_LEVEL_STANDARD, "console", aBuf);
			}
		}
		else if(Stroke)
		{
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "No such command: %s.", Result.m_pCommand);
			Print(OUTPUT_LEVEL_STANDARD, "console", aBuf);
		}

		pStr = pNextPart;
	}
}

int CConsole::PossibleCommands(const char *pStr, int FlagMask, bool Temp, FPossibleCallback pfnCallback, void *pUser)
{
	int Index = 0;
	for(CCommand *pCommand = m_pFirstCommand; pCommand; pCommand = pCommand->m_pNext)
	{
		if(pCommand->m_Flags & FlagMask && pCommand->m_Temp == Temp && str_find_nocase(pCommand->m_pName, pStr))
		{
			pfnCallback(Index, pCommand->m_pName, pUser);
			Index++;
		}
	}
	return Index;
}

int CConsole::PossibleMaps(const char *pStr, FPossibleCallback pfnCallback, void *pUser)
{
	int Index = 0;
	for(CMapListEntryTemp *pMapEntry = m_pFirstMapEntry; pMapEntry; pMapEntry = pMapEntry->m_pNext)
	{
		if(str_find_nocase(pMapEntry->m_aName, pStr))
		{
			pfnCallback(Index, pMapEntry->m_aName, pUser);
			Index++;
		}
	}
	return Index;
}

CConsole::CCommand *CConsole::FindCommand(const char *pName, int FlagMask)
{
	for(CCommand *pCommand = m_pFirstCommand; pCommand; pCommand = pCommand->m_pNext)
	{
		if(pCommand->m_Flags & FlagMask && str_comp_nocase(pCommand->m_pName, pName) == 0)
		{
			return pCommand;
		}
	}

	return 0x0;
}

void CConsole::ExecuteLine(const char *pStr)
{
	CConsole::ExecuteLineStroked(1, pStr); // press it
	CConsole::ExecuteLineStroked(0, pStr); // then release it
}

void CConsole::ExecuteLineFlag(const char *pStr, int FlagMask)
{
	int Temp = m_FlagMask;
	m_FlagMask = FlagMask;
	ExecuteLine(pStr);
	m_FlagMask = Temp;
}

bool CConsole::ExecuteFile(const char *pFilename)
{
	// make sure that this isn't being executed already
	for(CExecFile *pCur = m_pFirstExec; pCur; pCur = pCur->m_pPrev)
		if(str_comp(pFilename, pCur->m_pFilename) == 0)
			return false;

	if(!m_pStorage)
		return false;

	// push this one to the stack
	CExecFile ThisFile;
	CExecFile *pPrev = m_pFirstExec;
	ThisFile.m_pFilename = pFilename;
	ThisFile.m_pPrev = m_pFirstExec;
	m_pFirstExec = &ThisFile;

	// exec the file
	IOHANDLE File = m_pStorage->OpenFile(pFilename, IOFLAG_READ | IOFLAG_SKIP_BOM, IStorage::TYPE_ALL);

	char aBuf[256];
	if(File)
	{
		str_format(aBuf, sizeof(aBuf), "executing '%s'", pFilename);
		Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", aBuf);

		CLineReader LineReader;
		LineReader.Init(File);
		const char *pLine;
		while((pLine = LineReader.Get()))
			ExecuteLine(pLine);

		io_close(File);
	}
	else
	{
		str_format(aBuf, sizeof(aBuf), "failed to open '%s'", pFilename);
		Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", aBuf);
		bool AbsHeur = false;
		AbsHeur = AbsHeur || (pFilename[0] == '/' || pFilename[0] == '\\');
		AbsHeur = AbsHeur || (pFilename[0] && pFilename[1] == ':' && (pFilename[2] == '/' || pFilename[2] == '\\'));
		if(AbsHeur)
		{
			Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", "Info: only relative paths starting from the ones you specify in 'storage.cfg' are allowed");
		}
	}

	m_pFirstExec = pPrev;
	return (bool) File;
}

void CConsole::Con_Echo(IResult *pResult, void *pUserData)
{
	((CConsole *) pUserData)->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", pResult->GetString(0));
}

void CConsole::Con_Exec(IResult *pResult, void *pUserData)
{
	((CConsole *) pUserData)->ExecuteFile(pResult->GetString(0));
}

void CConsole::ConModCommandAccess(IResult *pResult, void *pUser)
{
	CConsole *pConsole = static_cast<CConsole *>(pUser);
	char aBuf[128];
	CCommand *pCommand = pConsole->FindCommand(pResult->GetString(0), CFGFLAG_SERVER);
	if(pCommand)
	{
		if(pResult->NumArguments() == 2)
		{
			pCommand->SetAccessLevel(pResult->GetInteger(1));
			str_format(aBuf, sizeof(aBuf), "moderator access for '%s' is now %s", pResult->GetString(0), pCommand->GetAccessLevel() ? "enabled" : "disabled");
		}
		else
			str_format(aBuf, sizeof(aBuf), "moderator access for '%s' is %s", pResult->GetString(0), pCommand->GetAccessLevel() ? "enabled" : "disabled");
	}
	else
		str_format(aBuf, sizeof(aBuf), "No such command: '%s'.", pResult->GetString(0));

	pConsole->Print(OUTPUT_LEVEL_STANDARD, "console", aBuf);
}

void CConsole::ConModCommandStatus(IResult *pResult, void *pUser)
{
	CConsole *pConsole = static_cast<CConsole *>(pUser);
	char aBuf[240];
	mem_zero(aBuf, sizeof(aBuf));
	int Used = 0;

	for(CCommand *pCommand = pConsole->m_pFirstCommand; pCommand; pCommand = pCommand->m_pNext)
	{
		if(pCommand->m_Flags & pConsole->m_FlagMask && pCommand->GetAccessLevel() == ACCESS_LEVEL_MOD)
		{
			int Length = str_length(pCommand->m_pName);
			if(Used + Length + 2 < (int) (sizeof(aBuf)))
			{
				if(Used > 0)
				{
					Used += 2;
					str_append(aBuf, ", ", sizeof(aBuf));
				}
				str_append(aBuf, pCommand->m_pName, sizeof(aBuf));
				Used += Length;
			}
			else
			{
				pConsole->Print(OUTPUT_LEVEL_STANDARD, "console", aBuf);
				mem_zero(aBuf, sizeof(aBuf));
				str_copy(aBuf, pCommand->m_pName, sizeof(aBuf));
				Used = Length;
			}
		}
	}
	if(Used > 0)
		pConsole->Print(OUTPUT_LEVEL_STANDARD, "console", aBuf);
}

struct CIntVariableData
{
	IConsole *m_pConsole;
	int *m_pVariable;
	int m_Min;
	int m_Max;
};

struct CStrVariableData
{
	IConsole *m_pConsole;
	char *m_pStr;
	int m_MaxSize;
	int m_Length;
};

static void IntVariableCommand(IConsole::IResult *pResult, void *pUserData)
{
	CIntVariableData *pData = (CIntVariableData *) pUserData;

	if(pResult->NumArguments())
	{
		int Val = pResult->GetInteger(0);

		// do clamping
		if(pData->m_Min != pData->m_Max)
		{
			if(Val < pData->m_Min)
				Val = pData->m_Min;
			if(pData->m_Max != 0 && Val > pData->m_Max)
				Val = pData->m_Max;
		}

		*(pData->m_pVariable) = Val;
	}
	else
	{
		char aBuf[1024];
		str_format(aBuf, sizeof(aBuf), "Value: %d", *(pData->m_pVariable));
		pData->m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", aBuf);
		pResult->m_Value = *(pData->m_pVariable);
	}
}

static void StrVariableCommand(IConsole::IResult *pResult, void *pUserData)
{
	CStrVariableData *pData = (CStrVariableData *) pUserData;

	if(pResult->NumArguments())
	{
		const char *pString = pResult->GetString(0);
		if(!str_utf8_check(pString))
		{
			char Temp[4];
			int Length = 0;
			while(*pString)
			{
				int Size = str_utf8_encode(Temp, static_cast<unsigned char>(*pString++));
				if(Length + Size < pData->m_MaxSize)
				{
					mem_copy(pData->m_pStr + Length, &Temp, Size);
					Length += Size;
				}
				else
					break;
			}
			pData->m_pStr[Length] = 0;
		}
		else
			str_utf8_copy_num(pData->m_pStr, pString, pData->m_MaxSize, pData->m_Length);
	}
	else
	{
		char aBuf[1024];
		str_format(aBuf, sizeof(aBuf), "Value: %s", pData->m_pStr);
		pData->m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", aBuf);
		str_copy(pResult->m_aValue, pData->m_pStr, sizeof(pResult->m_aValue));
	}
}

void CConsole::TraverseChain(FCommandCallback *ppfnCallback, void **ppUserData)
{
	while(*ppfnCallback == Con_Chain)
	{
		CChain *pChainInfo = static_cast<CChain *>(*ppUserData);
		*ppfnCallback = pChainInfo->m_pfnCallback;
		*ppUserData = pChainInfo->m_pCallbackUserData;
	}
}

void CConsole::Con_EvalIf(IResult *pResult, void *pUserData)
{
	CConsole *pConsole = static_cast<CConsole *>(pUserData);
	CCommand *pCommand = pConsole->FindCommand(pResult->GetString(0), pConsole->m_FlagMask);
	char aBuf[128];
	if(!pCommand)
	{
		str_format(aBuf, sizeof(aBuf), "No such command: '%s'.", pResult->GetString(0));
		pConsole->Print(OUTPUT_LEVEL_STANDARD, "console", aBuf);
		return;
	}
	FCommandCallback pfnCallback = pCommand->m_pfnCallback;
	void *pCallbackUserData = pCommand->m_pUserData;
	pConsole->TraverseChain(&pfnCallback, &pCallbackUserData);
	CResult Result;
	pfnCallback(&Result, pCallbackUserData);
	bool Condition = false;
	if(pfnCallback == IntVariableCommand)
		Condition = Result.m_Value == atoi(pResult->GetString(2));
	else
		Condition = !str_comp_nocase(Result.m_aValue, pResult->GetString(2));
	if(!str_comp(pResult->GetString(1), "!="))
		Condition = !Condition;
	else if(str_comp(pResult->GetString(1), "==") && pfnCallback == StrVariableCommand)
		pConsole->Print(OUTPUT_LEVEL_STANDARD, "console", "Error: invalid comperator for type string");
	else if(!str_comp(pResult->GetString(1), ">"))
		Condition = Result.m_Value > atoi(pResult->GetString(2));
	else if(!str_comp(pResult->GetString(1), "<"))
		Condition = Result.m_Value < atoi(pResult->GetString(2));
	else if(!str_comp(pResult->GetString(1), "<="))
		Condition = Result.m_Value <= atoi(pResult->GetString(2));
	else if(!str_comp(pResult->GetString(1), ">="))
		Condition = Result.m_Value >= atoi(pResult->GetString(2));
	else if(str_comp(pResult->GetString(1), "=="))
		pConsole->Print(OUTPUT_LEVEL_STANDARD, "console", "Error: invalid comperator for type integer");

	if(pResult->NumArguments() > 4 && str_comp(pResult->GetString(4), "else"))
		pConsole->Print(OUTPUT_LEVEL_STANDARD, "console", "Error: expected else");

	if(Condition)
		pConsole->ExecuteLine(pResult->GetString(3));
	else if(pResult->NumArguments() == 6)
		pConsole->ExecuteLine(pResult->GetString(5));
}

void CConsole::Con_EvalIfCmd(IResult *pResult, void *pUserData)
{
	CConsole *pConsole = static_cast<CConsole *>(pUserData);
	CCommand *pCommand = pConsole->FindCommand(pResult->GetString(0), pConsole->m_FlagMask);
	if(pResult->NumArguments() > 2 && str_comp(pResult->GetString(2), "else"))
		pConsole->Print(OUTPUT_LEVEL_STANDARD, "console", "Error: expected else");

	if(pCommand)
		pConsole->ExecuteLine(pResult->GetString(1));
	else if(pResult->NumArguments() == 4)
		pConsole->ExecuteLine(pResult->GetString(3));
}

void CConsole::ConToggle(IConsole::IResult *pResult, void *pUser)
{
	CConsole *pConsole = static_cast<CConsole *>(pUser);
	char aBuf[128] = {0};
	CCommand *pCommand = pConsole->FindCommand(pResult->GetString(0), pConsole->m_FlagMask);
	if(pCommand)
	{
		FCommandCallback pfnCallback = pCommand->m_pfnCallback;
		void *pUserData = pCommand->m_pUserData;
		pConsole->TraverseChain(&pfnCallback, &pUserData);
		if(pfnCallback == IntVariableCommand)
		{
			CIntVariableData *pData = static_cast<CIntVariableData *>(pUserData);
			int Val = *(pData->m_pVariable) == pResult->GetInteger(1) ? pResult->GetInteger(2) : pResult->GetInteger(1);
			str_format(aBuf, sizeof(aBuf), "%s %i", pResult->GetString(0), Val);
			pConsole->ExecuteLine(aBuf);
			aBuf[0] = 0;
		}
		else
			str_format(aBuf, sizeof(aBuf), "Invalid command: '%s'.", pResult->GetString(0));
	}
	else
		str_format(aBuf, sizeof(aBuf), "No such command: '%s'.", pResult->GetString(0));

	if(aBuf[0])
		pConsole->Print(OUTPUT_LEVEL_STANDARD, "console", aBuf);
}

void CConsole::ConToggleStroke(IConsole::IResult *pResult, void *pUser)
{
	CConsole *pConsole = static_cast<CConsole *>(pUser);
	char aBuf[128] = {0};
	CCommand *pCommand = pConsole->FindCommand(pResult->GetString(1), pConsole->m_FlagMask);
	if(pCommand)
	{
		FCommandCallback pfnCallback = pCommand->m_pfnCallback;
		void *pUserData = pCommand->m_pUserData;
		pConsole->TraverseChain(&pfnCallback, &pUserData);
		if(pfnCallback == IntVariableCommand)
		{
			int Val = pResult->GetInteger(0) == 0 ? pResult->GetInteger(3) : pResult->GetInteger(2);
			str_format(aBuf, sizeof(aBuf), "%s %i", pResult->GetString(1), Val);
			pConsole->ExecuteLine(aBuf);
			aBuf[0] = 0;
		}
		else
			str_format(aBuf, sizeof(aBuf), "Invalid command: '%s'.", pResult->GetString(1));
	}
	else
		str_format(aBuf, sizeof(aBuf), "No such command: '%s'.", pResult->GetString(1));

	if(aBuf[0])
		pConsole->Print(OUTPUT_LEVEL_STANDARD, "console", aBuf);
}

CConsole::CConsole(int FlagMask)
{
	m_FlagMask = FlagMask;
	m_AccessLevel = ACCESS_LEVEL_ADMIN;
	m_pRecycleList = 0;
	m_TempCommands.Reset();
	m_StoreCommands = true;
	m_apStrokeStr[0] = "0";
	m_apStrokeStr[1] = "1";
	m_pTempMapListHeap = 0;
	m_pFirstMapEntry = 0;
	m_pLastMapEntry = 0;
	m_ExecutionQueue.Reset();
	m_pFirstCommand = 0;
	m_pFirstExec = 0;
	mem_zero(m_aPrintCB, sizeof(m_aPrintCB));
	m_NumPrintCB = 0;

	m_pConfig = 0;
	m_pStorage = 0;

	// register some basic commands
	Register("echo", "r[text]", CFGFLAG_SERVER | CFGFLAG_CLIENT, Con_Echo, this, "Echo the text");
	Register("exec", "r[file]", CFGFLAG_SERVER | CFGFLAG_CLIENT, Con_Exec, this, "Execute the specified file");
	Register("eval_if", "s[config] s[comparison] s[value] s[command] ?s[else] ?s[command]", CFGFLAG_SERVER | CFGFLAG_CLIENT | CFGFLAG_STORE, Con_EvalIf, this, "Execute command if condition is true");
	Register("eval_if_cmd", "s[check_command] s[command] ?s[else] ?s[command]", CFGFLAG_SERVER | CFGFLAG_CLIENT | CFGFLAG_STORE, Con_EvalIfCmd, this, "Execute command if check_command exists");

	Register("toggle", "s[config-option] i[value1] i[value2]", CFGFLAG_SERVER | CFGFLAG_CLIENT, ConToggle, this, "Toggle config value");
	Register("+toggle", "s[config-option] i[value1] i[value2]", CFGFLAG_CLIENT, ConToggleStroke, this, "Toggle config value via keypress");

	Register("mod_command", "s[command] ?i[access-level]", CFGFLAG_SERVER, ConModCommandAccess, this, "Specify command accessibility for moderators");
	Register("mod_status", "", CFGFLAG_SERVER, ConModCommandStatus, this, "List all commands which are accessible for moderators");
}

CConsole::~CConsole()
{
	CCommand *pCommand = m_pFirstCommand;
	while(pCommand)
	{
		CCommand *pNext = pCommand->m_pNext;

		FCommandCallback pfnCallback = pCommand->m_pfnCallback;
		void *pUserData = pCommand->m_pUserData;
		while(pfnCallback == Con_Chain)
		{
			CChain *pChainInfo = static_cast<CChain *>(pUserData);
			pfnCallback = pChainInfo->m_pfnCallback;
			pUserData = pChainInfo->m_pCallbackUserData;
			mem_free(pChainInfo);
		}

		mem_free(pCommand);

		pCommand = pNext;
	}
	if(m_pTempMapListHeap)
	{
		delete m_pTempMapListHeap;
		m_pTempMapListHeap = 0;
	}
}

void CConsole::Init()
{
	m_pConfig = Kernel()->RequestInterface<IConfigManager>()->Values();
	m_pStorage = Kernel()->RequestInterface<IStorage>();

// TODO: this should disappear
#define MACRO_CONFIG_INT(Name, ScriptName, Def, Min, Max, Flags, Desc) \
	{ \
		static CIntVariableData Data = {this, &m_pConfig->m_##Name, Min, Max}; \
		Register(#ScriptName, "?i", Flags, IntVariableCommand, &Data, Desc); \
	}

#define MACRO_CONFIG_STR(Name, ScriptName, Len, Def, Flags, Desc) \
	{ \
		static CStrVariableData Data = {this, m_pConfig->m_##Name, Len, Len}; \
		Register(#ScriptName, "?r", Flags, StrVariableCommand, &Data, Desc); \
	}

#define MACRO_CONFIG_UTF8STR(Name, ScriptName, Size, Len, Def, Flags, Desc) \
	{ \
		static CStrVariableData Data = {this, m_pConfig->m_##Name, Size, Len}; \
		Register(#ScriptName, "?r", Flags, StrVariableCommand, &Data, Desc); \
	}

#include "config_variables.h"

#undef MACRO_CONFIG_INT
#undef MACRO_CONFIG_STR
#undef MACRO_CONFIG_UTF8STR
}

void CConsole::ParseArguments(int NumArgs, const char **ppArguments)
{
	for(int i = 0; i < NumArgs; i++)
	{
		// check for scripts to execute
		if(str_comp("-f", ppArguments[i]) == 0)
		{
			if(NumArgs - i > 1)
				ExecuteFile(ppArguments[i + 1]);
			i++;
		}
		else if(!str_comp("-s", ppArguments[i]) || !str_comp("--silent", ppArguments[i]) ||
			!str_comp("-d", ppArguments[i]) || !str_comp("--default", ppArguments[i]))
		{
			// skip silent, default param
			continue;
		}
		else
		{
			// search arguments for overrides
			ExecuteLine(ppArguments[i]);
		}
	}
}

void CConsole::AddCommandSorted(CCommand *pCommand)
{
	if(!m_pFirstCommand || str_comp(pCommand->m_pName, m_pFirstCommand->m_pName) <= 0)
	{
		if(m_pFirstCommand && m_pFirstCommand->m_pNext)
			pCommand->m_pNext = m_pFirstCommand;
		else
			pCommand->m_pNext = 0;
		m_pFirstCommand = pCommand;
	}
	else
	{
		for(CCommand *p = m_pFirstCommand; p; p = p->m_pNext)
		{
			if(!p->m_pNext || str_comp(pCommand->m_pName, p->m_pNext->m_pName) <= 0)
			{
				pCommand->m_pNext = p->m_pNext;
				p->m_pNext = pCommand;
				break;
			}
		}
	}
}

void CConsole::Register(const char *pName, const char *pParams,
	int Flags, FCommandCallback pfnFunc, void *pUser, const char *pHelp)
{
	CCommand *pCommand = FindCommand(pName, Flags);
	bool DoAdd = false;
	if(pCommand == 0)
	{
		pCommand = new(mem_alloc(sizeof(CCommand))) CCommand(Flags & CFGFLAG_BASICACCESS);
		;
		DoAdd = true;
	}
	pCommand->m_pfnCallback = pfnFunc;
	pCommand->m_pUserData = pUser;

	pCommand->m_pName = pName;
	pCommand->m_pHelp = pHelp;
	pCommand->m_pParams = pParams;

	pCommand->m_Flags = Flags;
	pCommand->m_Temp = false;

	if(DoAdd)
		AddCommandSorted(pCommand);
}

void CConsole::RegisterTemp(const char *pName, const char *pParams, int Flags, const char *pHelp)
{
	CCommand *pCommand;
	if(m_pRecycleList)
	{
		pCommand = m_pRecycleList;
		str_copy(const_cast<char *>(pCommand->m_pName), pName, TEMPCMD_NAME_LENGTH);
		str_copy(const_cast<char *>(pCommand->m_pHelp), pHelp, TEMPCMD_HELP_LENGTH);
		str_copy(const_cast<char *>(pCommand->m_pParams), pParams, TEMPCMD_PARAMS_LENGTH);

		m_pRecycleList = m_pRecycleList->m_pNext;
	}
	else
	{
		pCommand = new(m_TempCommands.Allocate(sizeof(CCommand))) CCommand(false);
		char *pMem = static_cast<char *>(m_TempCommands.Allocate(TEMPCMD_NAME_LENGTH));
		str_copy(pMem, pName, TEMPCMD_NAME_LENGTH);
		pCommand->m_pName = pMem;
		pMem = static_cast<char *>(m_TempCommands.Allocate(TEMPCMD_HELP_LENGTH));
		str_copy(pMem, pHelp, TEMPCMD_HELP_LENGTH);
		pCommand->m_pHelp = pMem;
		pMem = static_cast<char *>(m_TempCommands.Allocate(TEMPCMD_PARAMS_LENGTH));
		str_copy(pMem, pParams, TEMPCMD_PARAMS_LENGTH);
		pCommand->m_pParams = pMem;
	}

	pCommand->m_pfnCallback = 0;
	pCommand->m_pUserData = 0;
	pCommand->m_Flags = Flags;
	pCommand->m_Temp = true;

	AddCommandSorted(pCommand);
}

void CConsole::DeregisterTemp(const char *pName)
{
	if(!m_pFirstCommand)
		return;

	CCommand *pRemoved = 0;

	// remove temp entry from command list
	if(m_pFirstCommand->m_Temp && str_comp(m_pFirstCommand->m_pName, pName) == 0)
	{
		pRemoved = m_pFirstCommand;
		m_pFirstCommand = m_pFirstCommand->m_pNext;
	}
	else
	{
		for(CCommand *pCommand = m_pFirstCommand; pCommand->m_pNext; pCommand = pCommand->m_pNext)
			if(pCommand->m_pNext->m_Temp && str_comp(pCommand->m_pNext->m_pName, pName) == 0)
			{
				pRemoved = pCommand->m_pNext;
				pCommand->m_pNext = pCommand->m_pNext->m_pNext;
				break;
			}
	}

	// add to recycle list
	if(pRemoved)
	{
		pRemoved->m_pNext = m_pRecycleList;
		m_pRecycleList = pRemoved;
	}
}

void CConsole::DeregisterTempAll()
{
	// set non temp as first one
	for(; m_pFirstCommand && m_pFirstCommand->m_Temp; m_pFirstCommand = m_pFirstCommand->m_pNext)
		;

	// remove temp entries from command list
	for(CCommand *pCommand = m_pFirstCommand; pCommand && pCommand->m_pNext; pCommand = pCommand->m_pNext)
	{
		CCommand *pNext = pCommand->m_pNext;
		if(pNext->m_Temp)
		{
			for(; pNext && pNext->m_Temp; pNext = pNext->m_pNext)
				;
			pCommand->m_pNext = pNext;
		}
	}

	m_TempCommands.Reset();
	m_pRecycleList = 0;
}

void CConsole::RegisterTempMap(const char *pName)
{
	if(!m_pTempMapListHeap)
		m_pTempMapListHeap = new CHeap();
	CMapListEntryTemp *pEntry = (CMapListEntryTemp *) m_pTempMapListHeap->Allocate(sizeof(CMapListEntryTemp));
	pEntry->m_pNext = 0;
	pEntry->m_pPrev = m_pLastMapEntry;
	if(pEntry->m_pPrev)
		pEntry->m_pPrev->m_pNext = pEntry;
	m_pLastMapEntry = pEntry;
	if(!m_pFirstMapEntry)
		m_pFirstMapEntry = pEntry;
	str_copy(pEntry->m_aName, pName, TEMPMAP_NAME_LENGTH);
}

void CConsole::DeregisterTempMap(const char *pName)
{
	if(!m_pFirstMapEntry)
		return;

	CHeap *pNewTempMapListHeap = new CHeap();
	CMapListEntryTemp *pNewFirstEntry = 0;
	CMapListEntryTemp *pNewLastEntry = 0;

	for(CMapListEntryTemp *pSrc = m_pFirstMapEntry; pSrc; pSrc = pSrc->m_pNext)
	{
		if(str_comp_nocase(pName, pSrc->m_aName) == 0)
			continue;

		CMapListEntryTemp *pDst = (CMapListEntryTemp *) pNewTempMapListHeap->Allocate(sizeof(CMapListEntryTemp));
		pDst->m_pNext = 0;
		pDst->m_pPrev = m_pLastMapEntry;
		if(pDst->m_pPrev)
			pDst->m_pPrev->m_pNext = pDst;
		m_pLastMapEntry = pDst;
		if(!m_pFirstMapEntry)
			m_pFirstMapEntry = pDst;

		str_copy(pDst->m_aName, pSrc->m_aName, TEMPMAP_NAME_LENGTH);
	}

	delete m_pTempMapListHeap;
	m_pTempMapListHeap = pNewTempMapListHeap;
	m_pFirstMapEntry = pNewFirstEntry;
	m_pLastMapEntry = pNewLastEntry;
}

void CConsole::DeregisterTempMapAll()
{
	if(m_pTempMapListHeap)
		m_pTempMapListHeap->Reset();
	m_pFirstMapEntry = 0;
	m_pLastMapEntry = 0;
}

void CConsole::Con_Chain(IResult *pResult, void *pUserData)
{
	CChain *pInfo = (CChain *) pUserData;
	pInfo->m_pfnChainCallback(pResult, pInfo->m_pUserData, pInfo->m_pfnCallback, pInfo->m_pCallbackUserData);
}

void CConsole::Chain(const char *pName, FChainCommandCallback pfnChainFunc, void *pUser)
{
	CCommand *pCommand = FindCommand(pName, m_FlagMask);

	if(!pCommand)
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "failed to chain '%s'", pName);
		Print(IConsole::OUTPUT_LEVEL_DEBUG, "console", aBuf);
		return;
	}

	CChain *pChainInfo = (CChain *) mem_alloc(sizeof(CChain));

	// store info
	pChainInfo->m_pfnChainCallback = pfnChainFunc;
	pChainInfo->m_pUserData = pUser;
	pChainInfo->m_pfnCallback = pCommand->m_pfnCallback;
	pChainInfo->m_pCallbackUserData = pCommand->m_pUserData;

	// chain
	pCommand->m_pfnCallback = Con_Chain;
	pCommand->m_pUserData = pChainInfo;
}

void CConsole::StoreCommands(bool Store)
{
	if(!Store)
	{
		for(CExecutionQueue::CQueueEntry *pEntry = m_ExecutionQueue.m_pFirst; pEntry; pEntry = pEntry->m_pNext)
			pEntry->m_pCommand->m_pfnCallback(&pEntry->m_Result, pEntry->m_pCommand->m_pUserData);
		m_ExecutionQueue.Reset();
	}
	m_StoreCommands = Store;
}

const IConsole::CCommandInfo *CConsole::GetCommandInfo(const char *pName, int FlagMask, bool Temp)
{
	for(CCommand *pCommand = m_pFirstCommand; pCommand; pCommand = pCommand->m_pNext)
	{
		if(pCommand->m_Flags & FlagMask && pCommand->m_Temp == Temp)
		{
			if(str_comp_nocase(pCommand->m_pName, pName) == 0)
				return pCommand;
		}
	}

	return 0;
}

extern IConsole *CreateConsole(int FlagMask) { return new CConsole(FlagMask); }
