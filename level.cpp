#include "driver_level.h"
#include "IFileSystem.h"
#include "VirtualStream.h"

ConVar g_format("format", "2");

//--------------------------------------------------------------------------------------------------------------------------

EqString			g_levname = "MIAMI.LEV";

IVirtualStream*		g_levStream = NULL;

DkList<EqString>	g_model_names;
DkList<EqString>	g_texture_names;
char*				g_textureNamesData = NULL;

DkList<ModelRef_t>	g_levelModels;		// level models

CarModelData_t		g_carModels[MAX_CAR_MODELS];

MODEL* FindModelByIndex(int nIndex, RegionModels_t* models)
{
	if(nIndex >= 0 && nIndex < g_levelModels.numElem())
	{
		// try searching in region datas
		if(g_levelModels[nIndex].swap && models)
		{
			for(int i = 0; i < models->modelRefs.numElem(); i++)
			{
				if(models->modelRefs[i].index == nIndex)
					return models->modelRefs[i].model;
			}
		}

		return g_levelModels[nIndex].model;
	}

	return NULL;
}

int GetModelIndexByName(const char* name)
{
	for(int i = 0; i < g_levelModels.numElem(); i++)
	{
		if(!strcmp(g_model_names[i].c_str(), name))
			return i;
	}

	return -1;
}

//-------------------------------------------------------------
// parses model lumps and exports models to OBJ
//-------------------------------------------------------------
void LoadCarModelsLump(IVirtualStream* pFile, int ofs, int size)
{
	int l_ofs = pFile->Tell();

	int modelCount;
	pFile->Read(&modelCount, sizeof(int), 1);

	Msg("	all car models count: %d\n", modelCount);

	// read entries
	carmodelentry_t model_entries[MAX_CAR_MODELS];
	pFile->Read(&model_entries, sizeof(carmodelentry_t), MAX_CAR_MODELS);

	// position
	int r_ofs = pFile->Tell();

	int pad; // really padding?
	pFile->Read(&pad, sizeof(int), 1);

	// load car models
	for(int i = 0; i < MAX_CAR_MODELS; i++)
	{
		if(model_entries[i].cleanOffset != -1)
		{
			pFile->Seek(r_ofs+model_entries[i].cleanOffset, VS_SEEK_SET);

			pFile->Read(&g_carModels[i].cleanSize, 1, sizeof(int));

			g_carModels[i].cleanmodel = (MODEL*)malloc(g_carModels[i].cleanSize);
			pFile->Read(g_carModels[i].cleanmodel, 1, g_carModels[i].cleanSize);
		}
		else
			g_carModels[i].cleanmodel = NULL;

		if(model_entries[i].damOffset != -1)
		{
			pFile->Seek(r_ofs+model_entries[i].damOffset, VS_SEEK_SET);

			pFile->Read(&g_carModels[i].damSize, 1, sizeof(int));

			g_carModels[i].dammodel = (MODEL*)malloc(g_carModels[i].damSize);
			pFile->Read(g_carModels[i].dammodel, 1, g_carModels[i].damSize);
		}
		else
			g_carModels[i].dammodel = NULL;

		if(model_entries[i].lowOffset != -1)
		{
			pFile->Seek(r_ofs+model_entries[i].lowOffset, VS_SEEK_SET);

			pFile->Read(&g_carModels[i].lowSize, 1, sizeof(int));

			g_carModels[i].lowmodel = (MODEL*)malloc(g_carModels[i].lowSize);
			pFile->Read(g_carModels[i].lowmodel, 1, g_carModels[i].lowSize);
		}
		else
			g_carModels[i].lowmodel = NULL;
	}

	pFile->Seek(l_ofs, VS_SEEK_SET);
}

//-------------------------------------------------------------
// load model names
//-------------------------------------------------------------
void LoadModelNamesLump(IVirtualStream* pFile, int size)
{
	int l_ofs = pFile->Tell();

	char* modelnames = new char[size];
	pFile->Read(modelnames, size, 1);

	int len = strlen(modelnames);
	int sz = 0;

	do
	{
		char* str = modelnames+sz;

		len = strlen(str);

		g_model_names.append(str);

		sz += len + 1; 
	}while(sz < size);

	delete [] modelnames;

	pFile->Seek(l_ofs, VS_SEEK_SET);
}

//-------------------------------------------------------------
// load texture names, same as model names
//-------------------------------------------------------------
void LoadTextureNamesLump(IVirtualStream* pFile, int size)
{
	int l_ofs = pFile->Tell();

	g_textureNamesData = new char[size];

	pFile->Read(g_textureNamesData, size, 1);

	int len = strlen(g_textureNamesData);
	int sz = 0;

	do
	{
		char* str = g_textureNamesData+sz;

		len = strlen(str);

		g_texture_names.append(str);

		sz += len + 1; 
	}while(sz < size);

	pFile->Seek(l_ofs, VS_SEEK_SET);
}

//-------------------------------------------------------------
// parses model lumps and exports models to OBJ
//-------------------------------------------------------------
void LoadLevelModelsLump(IVirtualStream* pFile)
{
	int l_ofs = pFile->Tell();

	int modelCount;
	pFile->Read(&modelCount, sizeof(int), 1);

	MsgInfo("	model count: %d\n", modelCount);

	for(int i = 0; i < modelCount; i++)
	{
		int modelSize;

		pFile->Read(&modelSize, sizeof(int), 1);

		if(modelSize > 0)
		{
			char* data = (char*)malloc(modelSize);

			pFile->Read(data, modelSize, 1);

			ModelRef_t ref;
			ref.index = i;
			ref.model = (MODEL*)data;
			ref.size = modelSize;
			ref.swap = false;

			g_levelModels.append(ref);
		}
		else // leave empty as swap
		{
			ModelRef_t ref;
			ref.index = i;
			ref.model = NULL;
			ref.size = modelSize;
			ref.swap = true;

			g_levelModels.append(ref);
		}
	}
	 
	pFile->Seek(l_ofs, VS_SEEK_SET);
}

//---------------------------------------------------------------------------------------------------------------------------------

void ReadLevelChunk(IVirtualStream* pFile)
{
	int lump_count = 255; // Driver 2 difference: you not need to read lump count

	if(g_format.GetInt() == 1)
		pFile->Read(&lump_count, sizeof(int), 1);	// uncomment to support driver 1 TODO: command line switch

	LUMP lump;

	Msg("FILE OFS: %d bytes\n", pFile->Tell());

	for(int i = 0; i < lump_count; i++)
	{
		// read lump info
		pFile->Read(&lump, sizeof(LUMP), 1);

		// stop marker
		if(lump.type == 255)
			break;

		switch(lump.type)
		{
			case LUMP_MODELS:
				MsgWarning("LUMP_MODELS ofs=%d size=%d\n", pFile->Tell(), lump.size);
				LoadLevelModelsLump(pFile);
				break;
			case LUMP_MAP:
				MsgWarning("LUMP_MAP ofs=%d size=%d\n", pFile->Tell(), lump.size);
				LoadMapLump(pFile);
				break;
			case LUMP_TEXTURENAMES:
				MsgWarning("LUMP_TEXTURENAMES ofs=%d size=%d\n", pFile->Tell(), lump.size);
				LoadTextureNamesLump(pFile, lump.size);
				break;
			case LUMP_MODELNAMES:
				MsgWarning("LUMP_MODELNAMES ofs=%d size=%d\n", pFile->Tell(), lump.size);
				LoadModelNamesLump(pFile, lump.size);
				break;
			case LUMP_SUBDIVISION:
				MsgWarning("LUMP_SUBDIVISION ofs=%d size=%d\n", pFile->Tell(), lump.size);
				break;
			case LUMP_LOWDETAILTABLE:
				MsgWarning("LUMP_LOWDETAILTABLE ofs=%d size=%d\n", pFile->Tell(), lump.size);
				break;
			case LUMP_MOTIONCAPTURE:
				MsgWarning("LUMP_MOTIONCAPTURE ofs=%d size=%d\n", pFile->Tell(), lump.size);
				break;
			case LUMP_OVERLAYMAP:
				MsgWarning("LUMP_OVERLAYMAP ofs=%d size=%d\n", pFile->Tell(), lump.size);
				break;
			case LUMP_PALLET:
				MsgWarning("LUMP_PALLET ofs=%d size=%d\n", pFile->Tell(), lump.size);
				break;
			case LUMP_SPOOLINFO:
				MsgWarning("LUMP_SPOOLINFO ofs=%d size=%d\n", pFile->Tell(), lump.size);
				LoadSpoolInfoLump(pFile);
				break;
			case LUMP_STRAIGHTS2:
				MsgWarning("LUMP_STRAIGHTS2 ofs=%d size=%d\n", pFile->Tell(), lump.size);
				break;
			case LUMP_CURVES2:
				MsgWarning("LUMP_CURVES2 ofs=%d size=%d\n", pFile->Tell(), lump.size);
				break;
			case LUMP_JUNCTIONS2:
				MsgWarning("LUMP_JUNCTIONS2 ofs=%d size=%d\n", pFile->Tell(), lump.size);
				break;
			case LUMP_CHAIR:
				MsgWarning("LUMP_CHAIR ofs=%d size=%d\n", pFile->Tell(), lump.size);
				break;
			case LUMP_CAR_MODELS:
				MsgWarning("LUMP_CAR_MODELS ofs=%d size=%d\n", pFile->Tell(), lump.size);
				LoadCarModelsLump(pFile, pFile->Tell(), lump.size);
				break;
			case LUMP_TEXTUREINFO:
				MsgWarning("LUMP_TEXTUREINFO ofs=%d size=%d\n", pFile->Tell(), lump.size);
				LoadTextureInfoLump(pFile);
				break;
			default:
				MsgInfo("LUMP type: %d (0x%X) ofs=%d size=%d\n", lump.type, lump.type, pFile->Tell(), lump.size);
		}

		// skip lump
		pFile->Seek(lump.size, VS_SEEK_CUR);

		// position alignment
        if((pFile->Tell()%4) != 0)
			pFile->Seek(4-(pFile->Tell()%4),VS_SEEK_CUR);
	}
}

void LoadLevelFile(const char* filename)
{
	// try load driver2 lev file
	IFile* pReadFile = GetFileSystem()->Open(filename, "r+b");

	if(!pReadFile)
		return;

	CMemoryStream* stream = new CMemoryStream();
	stream->Open(NULL, VS_OPEN_WRITE | VS_OPEN_READ, 0);

	// read file into stream
	if( stream->ReadFromFileStream(pReadFile) )
	{
		GetFileSystem()->Close(pReadFile);
		g_levStream = stream;
	}
	else
	{
		delete stream;
		return;
	}

	// seek to begin
	g_levStream->Seek(0, VS_SEEK_SET);

	MsgWarning("-----------\nloading lev file '%s'\n", filename);

	EqString fileName = filename;

	g_levname = fileName.Path_Strip_Ext();

	//-------------------------------------------------------------------

	LUMP curLump;

	g_levStream->Read(&curLump, sizeof(curLump), 1);

	if(curLump.type != LUMP_LUMPDESC)
	{
		MsgError("Not a LEV file!\n");
		return;
	}

	// read chunk offsets
	g_levStream->Read(&g_levInfo, sizeof(dlevinfo_t), 1);

	Msg("levdesc_offset = %d\n", g_levInfo.levdesc_offset);
	Msg("levdesc_size = %d\n", g_levInfo.levdesc_size);

	Msg("texdata_offset = %d\n", g_levInfo.texdata_offset);
	Msg("texdata_size = %d\n", g_levInfo.texdata_size);

	Msg("levdata_offset = %d\n", g_levInfo.levdata_offset);
	Msg("levdata_size = %d\n", g_levInfo.levdata_size);

	Msg("spooldata_offset = %d\n", g_levInfo.spooldata_offset);
	Msg("spooldata_size = %d\n", g_levInfo.spooldata_size);

	// read cells
	
	//-----------------------------------------------------
	// seek to section 1 - lump data 1
	g_levStream->Seek( g_levInfo.levdesc_offset, VS_SEEK_SET );

	// read lump
	g_levStream->Read( &curLump, sizeof(curLump), 1 );

	if(curLump.type != LUMP_LEVELDESC)
	{
		MsgError("Not a LUMP_LEVELDESC!\n");
	}

	MsgInfo("entering LUMP_LEVELDESC size = %d\n--------------\n", curLump.size);

	// read sublumps
	ReadLevelChunk( g_levStream );

	//-----------------------------------------------------
	// read global textures

	LoadGlobalTextures(g_levStream);

	//-----------------------------------------------------
	// seek to section 3 - lump data 2
	g_levStream->Seek( g_levInfo.levdata_offset, VS_SEEK_SET );

	// read lump
	g_levStream->Read(&curLump, sizeof(curLump), 1);

	if(curLump.type != LUMP_LEVELDATA)
	{
		MsgError("Not a lump 0x24!\n");
	}

	MsgInfo("entering LUMP_LEVELDATA size = %d\n--------------\n", curLump.size);

	// read sublumps
	ReadLevelChunk( g_levStream );
}

void FreeLevelData()
{
	MsgWarning("FreeLevelData() ...\n");

	if(g_levStream)
		delete g_levStream;
	g_levStream = NULL;

	FreeSpoolData();

	for(int i = 0; i < g_levelModels.numElem(); i++)
	{
		if(g_levelModels[i].model)
			free(g_levelModels[i].model);
	}

	delete [] g_textureNamesData;

	// delete texture data
	for(int i = 0; i < g_numTexPages; i++)
	{
		if(g_pageDatas[i].data)
			delete [] g_pageDatas[i].data;

		delete [] g_texPages[i].details;
	}

	delete [] g_pageDatas;
	delete [] g_texPageInfos;
	delete [] g_texPages;

	g_pageDatas = NULL;
	g_texPageInfos = NULL;
	g_texPageInfos = NULL;
}
