#include "StdAfx.h"
#include "Resource.h"
#include "Utils.h"
#include "FileTextLines.h"
#include ".\patch.h"

CPatch::CPatch(void)
{
}

CPatch::~CPatch(void)
{
	FreeMemory();
}

void CPatch::FreeMemory()
{
	for (int i=0; i<m_arFileDiffs.GetCount(); i++)
	{
		Chunks * chunks = m_arFileDiffs.GetAt(i);
		for (int j=0; j<chunks->chunks.GetCount(); j++)
		{
			delete chunks->chunks.GetAt(j);
		}
		chunks->chunks.RemoveAll();
		delete chunks;
	} // iffs.GetCount(); i++) 
	m_arFileDiffs.RemoveAll();
}

BOOL CPatch::OpenUnifiedDiffFile(CString filename)
{
	CString sLine;
	INT_PTR nIndex = 0;
	INT_PTR nLineCount = 0;

	CStringArray PatchLines;
	FreeMemory();
	CStdioFile file(filename, CFile::typeText | CFile::modeRead | CFile::shareDenyNone);
	while (file.ReadString(sLine))
	{
		PatchLines.Add(sLine);
	}
	file.Close();
	nLineCount = PatchLines.GetCount();
	//now we got all the lines of the patch file
	//in our array - parsing can start...

	//first, skip possible garbage at the beginning
	//garbage is finished when a line starts with "Index: "
	//and the next line consists of only "=" characters
	for (; nIndex<PatchLines.GetCount(); nIndex++)
	{
		sLine = PatchLines.GetAt(nIndex);
		if (sLine.Left(7).Compare(_T("Index: "))==0)
		{
			if ((nIndex+1)<PatchLines.GetCount())
			{
				sLine = PatchLines.GetAt(nIndex+1);
				sLine.Replace(_T("="), _T(""));
				if (sLine.IsEmpty())
					break;
			} // if ((nIndex+1)<m_PatchLines.GetCount()) 
		} // if (sLine.Left(7).Compare(_T("Index: "))==0) 
	} // for (nIndex=0; nIndex<m_PatchLines.GetCount(); nIndex++) 
	if ((PatchLines.GetCount()-nIndex) < 2)
	{
		//no file entry found.
		m_sErrorMessage.LoadString(IDS_ERR_PATCH_NOINDEX);
		goto errorcleanup;
	} // if ((m_PatchLines.GetCount()-nIndex) < 2)

	//from this point on we have the real unified diff data
	int state = 0;
	Chunks * chunks = NULL;
	Chunk * chunk = NULL;
	int nAddLineCount = 0;
	int nRemoveLineCount = 0;
	int nContextLineCount = 0;
	for ( ;nIndex<PatchLines.GetCount(); nIndex++)
	{
		sLine = PatchLines.GetAt(nIndex);
		switch (state)
		{
		case 0:	//Index: <filepath>
			{
				if (sLine.Left(7).Compare(_T("Index: "))==0)
				{
					if (chunks)
					{
						//this is a new filediff, so add the last one to 
						//our array.
						m_arFileDiffs.Add(chunks);
					} // if (chunks) 
					chunks = new Chunks();
					state++;
				} // if (sLine.Left(7).Compare(_T("Index: "))==0)
				else
				{
					//the line
					//Index: <filepath>
					//was not found at the start of a filediff!
					
					//maybe an empty line? 
					if (sLine.IsEmpty())
						break;			//then just try again
					//But before throwing an error, check first if 
					//instead of a new filediff we just have a new chunk:
					if (nIndex > 0)
					{
						nIndex--;
						state = 4;
					} // if (nIndex > 0) 
					else
					{
						m_sErrorMessage.LoadString(IDS_ERR_PATCH_NOINDEX);
						goto errorcleanup;
					}
				}
			} 
		break;
		case 1:	//====================
			{
				sLine.Replace(_T("="), _T(""));
				if (sLine.IsEmpty())
					state++;
				else
				{
					//the line
					//=========================
					//was not found
					m_sErrorMessage.Format(IDS_ERR_PATCH_NOEQUATIONCHARLINE, nIndex);
					goto errorcleanup;
				}
			}
		break;
		case 2:	//--- <filepath>
			{
				if (sLine.Left(3).Compare(_T("---"))!=0)
				{
					//no starting "---" found
					//seems to be either garbage or just
					//a binary file. So start over...
					state = 0;

					break;
					//m_sErrorMessage.Format(IDS_ERR_PATCH_NOREMOVEFILELINE, nIndex);
					//goto errorcleanup;
				} // if (sLine.Left(3).Compare(_T("---"))!=0)
				sLine = sLine.Mid(3);	//remove the "---"
				sLine =sLine.Trim();
				//at the end of the filepath there's a revision number...
				int bracket = sLine.ReverseFind('(');
				CString num = sLine.Mid(bracket);		//num = "(revision xxxxx)"
				num = num.Mid(num.Find(' '));
				num = num.Trim(_T(" )"));
				chunks->sRevision = num;
				chunks->sFilePath = sLine.Left(bracket-1).Trim();
				state++;
			}
		break;
		case 3:	//+++ <filepath>
			{
				if (sLine.Left(3).Compare(_T("+++"))!=0)
				{
					//no starting "+++" found
					m_sErrorMessage.Format(IDS_ERR_PATCH_NOADDFILELINE, nIndex);
					goto errorcleanup;
				} // if (sLine.Left(3).Compare(_T("---"))!=0)
				sLine = sLine.Mid(3);	//remove the "---"
				sLine =sLine.Trim();
				//at the end of the filepath there's a revision number...
				int bracket = sLine.ReverseFind('(');
				if (sLine.Left(bracket-1).Trim().Compare(chunks->sFilePath) != 0)
				{
					//--- filename did not match the +++ filename
					//since we don't support renaming of files throw an error
					m_sErrorMessage.LoadString(IDS_ERR_PATCH_RENAMINGNOTSUPPORTED);
					goto errorcleanup;
				} // if (sLine.Left(bracket-1).Trim().Compare(chunks->sFilePath) != 0) 
				state++;
			}
		break;
		case 4:	//@@ -xxx,xxx +xxx,xxx @@
			{
				//start of a new chunk
				if (sLine.Left(2).Compare(_T("@@"))!=0)
				{
					//chunk doesn't start with "@@"
					//so there's garbage in between two filediffs
					state = 0;
					if (chunk)
					{
						delete chunk;
						chunk = 0;
					} // if (chunk) 
					if (chunks)
					{
						for (int i=0; i<chunks->chunks.GetCount(); i++)
						{
							delete chunks->chunks.GetAt(i);
						}
						chunks->chunks.RemoveAll();
						delete chunks;
						chunks = NULL;
					} // if (chunks)
					break;		//skip the garbage
					//m_sErrorMessage.Format(IDS_ERR_PATCH_CHUNKSTARTNOTFOUND, nIndex);
					//goto errorcleanup;
				} // if (sLine.Left(2).Compare(_T("@@"))!=0) 
				sLine = sLine.Mid(2);
				chunk = new Chunk();
				chunk->lRemoveStart = (-_ttol(sLine));
				sLine = sLine.Mid(sLine.Find(',')+1);
				chunk->lRemoveLength = _ttol(sLine);
				sLine = sLine.Mid(sLine.Find(' ')+1);
				chunk->lAddStart = _ttol(sLine);
				sLine = sLine.Mid(sLine.Find(',')+1);
				chunk->lAddLength = _ttol(sLine);
				state++;
			}
		break;
		case 5: //[ |+|-] <sourceline>
			{
				//this line is either a context line (with a ' ' in front)
				//a line added (with a '+' in front)
				//or a removed line (with a '-' in front)
				TCHAR type;
				if (sLine.IsEmpty())
					type = ' ';
				else
					type = sLine.GetAt(0);
				if (type == ' ')
				{
					//it's a context line - we don't use them here right now
					//but maybe in the future the patch algorithm can be
					//extended to use those in case the file to patch has
					//already changed and no base file is around...
					chunk->arLines.Add(sLine.Mid(1));
					chunk->arLinesStates.Add(PATCHSTATE_CONTEXT);
					nContextLineCount++;
				} // if (type == ' ')
				else if (type == '-')
				{
					//a removed line
					chunk->arLines.Add(sLine.Mid(1));
					chunk->arLinesStates.Add(PATCHSTATE_REMOVED);
					nRemoveLineCount++;
				}
				else if (type == '+')
				{
					//an added line
					chunk->arLines.Add(sLine.Mid(1));
					chunk->arLinesStates.Add(PATCHSTATE_ADDED);
					nAddLineCount++;
				}
				else
				{
					//none of those lines! what the hell happened here?
					m_sErrorMessage.Format(IDS_ERR_PATCH_UNKOWNLINETYPE, nIndex);
					goto errorcleanup;
				}
				if ((chunk->lAddLength == (nAddLineCount + nContextLineCount)) &&
					chunk->lRemoveLength == (nRemoveLineCount + nContextLineCount))
				{
					//chunk is finished
					chunks->chunks.Add(chunk);
					chunk = NULL;
					nAddLineCount = 0;
					nContextLineCount = 0;
					nRemoveLineCount = 0;
					state = 0;
				}
			} 
		break;
		default:
			ASSERT(FALSE);
		} // switch (state) 
	} // for ( ;nIndex<m_PatchLines.GetCount(); nIndex++) 
	if (chunk)
	{
		m_sErrorMessage.LoadString(IDS_ERR_PATCH_CHUNKMISMATCH);
		goto errorcleanup;
	}
	m_arFileDiffs.Add(chunks);
	return TRUE;
errorcleanup:
	if (chunk)
		delete chunk;
	if (chunks)
	{
		for (int i=0; i<chunks->chunks.GetCount(); i++)
		{
			delete chunks->chunks.GetAt(i);
		}
		chunks->chunks.RemoveAll();
		delete chunks;
	} // if (chunks)
	FreeMemory();
	return FALSE;
}

CString CPatch::GetFilename(int nIndex)
{
	if (nIndex < 0)
		return _T("");
	if (nIndex < m_arFileDiffs.GetCount())
	{
		Chunks * c = m_arFileDiffs.GetAt(nIndex);
		return c->sFilePath;
	} // if (nIndex < m_arFileDiffs.GetCount())
	return _T("");
}

CString CPatch::GetRevision(int nIndex)
{
	if (nIndex < 0)
		return 0;
	if (nIndex < m_arFileDiffs.GetCount())
	{
		Chunks * c = m_arFileDiffs.GetAt(nIndex);
		return c->sRevision;
	} // if (nIndex < m_arFileDiffs.GetCount())
	return 0;
}

BOOL CPatch::PatchFile(CString sPath, CString sSavePath, CString sBaseFile)
{
	if (!PathFileExists(sPath) || PathIsDirectory(sPath))
	{
		m_sErrorMessage.Format(IDS_ERR_PATCH_INVALIDPATCHFILE, sPath);
		return FALSE;
	}
	int nIndex = -1;
	for (int i=0; i<GetNumberOfFiles(); i++)
	{
		CString temppath = sPath;
		CString temp = GetFilename(i);
		temppath.Replace('/', '\\');
		temp.Replace('/', '\\');
		temp = temp.Mid(temp.Find('\\')+1);
		temppath = temppath.Right(temp.GetLength());
		if (temp.CompareNoCase(temppath)==0)
		{
			if (nIndex < 0)
			{
				nIndex = i;
			}
			else
			{
				ASSERT(FALSE);
			}
		} // if (temp.CompareNoCase(temppath)==0) 
	} // for (int i=0; i<GetNumberOfFiles(); i++)
	if (nIndex < 0)
	{
		m_sErrorMessage.Format(IDS_ERR_PATCH_FILENOTINPATCH, sPath);
		return FALSE;
	}

	CString sLine;
	CString sPatchFile = sBaseFile.IsEmpty() ? sPath : sBaseFile;
	if (!PathFileExists(sPatchFile))
	{
		// The file to patch does not exist.
		// Create an empty file so the patching process still can
		// work, at least if the patch contains a new file.
		HANDLE hFile = CreateFile(sPatchFile, GENERIC_WRITE, NULL, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hFile == INVALID_HANDLE_VALUE)
		{
			m_sErrorMessage.Format(IDS_ERR_PATCH_INVALIDPATCHFILE, sPatchFile);
			return FALSE;
		}
		CloseHandle(hFile);
	}
	CFileTextLines PatchLines;
	PatchLines.Load(sPatchFile);

	Chunks * chunks = m_arFileDiffs.GetAt(nIndex);

	for (int i=0; i<chunks->chunks.GetCount(); i++)
	{
		Chunk * chunk = chunks->chunks.GetAt(i);
		LONG lRemoveLine = chunk->lRemoveStart;
		LONG lAddLine = chunk->lAddStart;
		for (int j=0; j<chunk->arLines.GetCount(); j++)
		{
			CString sPatchLine = chunk->arLines.GetAt(j);
			int nPatchState = (int)chunk->arLinesStates.GetAt(j);
			switch (nPatchState)
			{
			case PATCHSTATE_REMOVED:
				{
					if (sPatchLine.Compare(PatchLines.GetAt(lRemoveLine-1))!=0)
					{
						m_sErrorMessage.Format(IDS_ERR_PATCH_DOESNOTMATCH, sPatchLine, PatchLines.GetAt(lRemoveLine-1));
						return FALSE; 
					} // if (sPatchLine.Compare(PatchLines.GetAt(lRemoveLine-1))!=0) 
					if (lRemoveLine >= PatchLines.GetCount())
					{
						m_sErrorMessage.Format(IDS_ERR_PATCH_DOESNOTMATCH, sPatchLine, _T(""));
						return FALSE; 
					} // if (lRemoveLine >= PatchLines.GetCount()) 
					PatchLines.RemoveAt(lRemoveLine-1);
				} 
				break;
			case PATCHSTATE_ADDED:
				{
					PatchLines.InsertAt(lAddLine-1, sPatchLine);
					lAddLine++;
					lRemoveLine++;
				}
				break;
			case PATCHSTATE_CONTEXT:
				{
					if ((sPatchLine.Compare(PatchLines.GetAt(lRemoveLine-1))!=0) &&
						(sPatchLine.Compare(PatchLines.GetAt(lAddLine-1))!=0))
					{
						m_sErrorMessage.Format(IDS_ERR_PATCH_DOESNOTMATCH, sPatchLine, PatchLines.GetAt(lRemoveLine-1));
						return FALSE; 
					} 
					lAddLine++;
					lRemoveLine++;
				}
				break;
			default:
				ASSERT(FALSE);
				break;
			} // switch (nPatchState) 
		} // for (j=0; j<chunk->arLines.GetCount(); j++) 
	} // for (int i=0; i<chunks->chunks.GetCount(); i++) 
	if (!sSavePath.IsEmpty())
	{
		PatchLines.Save(sSavePath);
	} // if (!sSavePath.IsEmpty())
	return TRUE;
}











