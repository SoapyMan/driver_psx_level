#include "driver_level.h"
#include "core/cmdlib.h"
#include "core/VirtualStream.h"
#include <direct.h>

bool g_export_carmodels = false;
bool g_export_models = false;
bool g_extract_dmodels = false;

bool g_export_world = false;

bool g_export_textures = false;
bool g_explode_tpages = false;

bool g_export_overmap = false;

int g_overlaymap_width = 1;

extern int g_region_format;
extern int g_format;

//---------------------------------------------------------------------------------------------------------------------------------

std::string			g_levname_moddir;
std::string			g_levname_texdir;

int					g_levSize = 0;

const float			texelSize = 1.0f / 256.0f;
const float			halfTexelSize = texelSize * 0.5f;


//-------------------------------------------------------------
// Exports level data
//-------------------------------------------------------------
void ExportLevelData()
{
	Msg("-------------\nExporting level data\n-------------\n");

	if (g_export_models)
		ExportAllModels();

	if (g_export_carmodels)
		ExportAllCarModels();
	
	if (g_export_world)
		ExportRegions();

	if (g_export_textures)
		ExportAllTextures();

	if (g_export_overmap)
		ExportOverlayMap();

	Msg("Export done\n");
}

//-------------------------------------------------------------------------------------------------------------------------

void ProcessLevFile(const char* filename)
{
	FILE* levTest = fopen(filename, "rb");

	if (!levTest)
	{
		MsgError("LEV file '%s' does not exists!\n", filename);
		return;
	}

	fseek(levTest, 0, SEEK_END);
	g_levSize = ftell(levTest);
	fclose(levTest);

	g_levname = filename;

	size_t lastindex = g_levname.find_last_of(".");
	g_levname = g_levname.substr(0, lastindex);

	g_levname_moddir = g_levname + "_models";
	g_levname_texdir = g_levname + "_textures";

	_mkdir(g_levname_moddir.c_str());
	_mkdir(g_levname_texdir.c_str());

	LoadLevelFile(filename);

	ExportLevelData();

	FreeLevelData();
}

// 
void ConvertDModelFileToOBJ(const char* filename, const char* outputFilename)
{
	MODEL* model;
	int modelSize;

	FILE* fp;
	fp = fopen(filename, "rb");
	if (fp)
	{
		fseek(fp, 0, SEEK_END);
		modelSize = ftell(fp);
		fseek(fp, 0, SEEK_SET);

		model = (MODEL*)malloc(modelSize + 16);
		fread(model, 1, modelSize, fp);
		fclose(fp);
	}
	else
	{
		MsgError("Failed to open '%s'!\n", filename);
		return;
	}

	if (model->instance_number > 0)
	{
		MsgError("This model points to instance '%d' and cannot be exported!\n", model->instance_number);
		return;
	}

	ExportDMODELToOBJ(model, outputFilename, 0, modelSize);
}

//----------------------------------------------------------------------------------------

void PrintCommandLineArguments()
{
	const char* argumentsMessage =
		"Example: driver_psx_level <command> [arguments]\n\n\n"
		"  -lev <filename.LEV> \t: Specify level file you want to input\n\n"
		"  -format <n> \t\t: Specify level file format. 1 = Driver 1 LEV, 2 = Driver 2 LEV\n\n"
		"  -regformat <n> \t: Specify level region format. 1 = Driver 1, 2 = Driver 2 Demo (alpha 1.6), 3 = Driver 2 Retail\n\n"
		"  -textures <1/0> \t: Export textures (TGA)\n\n"
		"  -models <1/0> \t: Export models (OBJ\n\n"
		"  -carmodels <1/0> \t: Export car models (OBJ)\n\n"
		"  -world <1/0> \t\t: Export whole world regions (OBJ)\n\n"
		"  -extractmodels \t: Extracts DMODEL files instead of exporting to OBJ\n\n"
		"  -overmap <width> \t: Extract overlay map with specified width\n\n"
		"  -explodetpages \t: Extracts textures as separate TIM files instead of whole texture page exporting as TGA\n\n"
		"  -dmodel2obj <filename.DMODEL> <output> \t: converts DMODEL to OBJ file\n\n";

	MsgInfo(argumentsMessage);
}

int main(int argc, char* argv[])
{
	Install_ConsoleSpewFunction();

	Msg("---------------\ndriver_level - Driver 2 level loader\n---------------\n\n");

	if (argc == 0)
	{
		PrintCommandLineArguments();
		return 0;
	}

	std::string levName;

	for (int i = 1; i < argc; i++)
	{
		if (!stricmp(argv[i], "-format"))
		{
			g_format = atoi(argv[i + 1]);

			if (g_format == 1)
			{
				g_region_format = 1;
			}
			else
			{
				if (g_region_format == 1)
					g_region_format = 3;
			}
			i++;
		}
		else if (!stricmp(argv[i], "-regformat"))
		{
			g_region_format = atoi(argv[i + 1]);
			i++;
		}
		else if (!stricmp(argv[i], "-textures"))
		{
			g_export_textures = atoi(argv[i + 1]) > 0;
			i++;
		}
		else if (!stricmp(argv[i], "-world"))
		{
			g_export_world = atoi(argv[i + 1]) > 0;
			i++;
		}
		else if (!stricmp(argv[i], "-models"))
		{
			g_export_models = atoi(argv[i + 1]) > 0;
			i++;
		}
		else if (!stricmp(argv[i], "-carmodels"))
		{
			g_export_carmodels = atoi(argv[i + 1]) > 0;
			i++;
		}
		else if (!stricmp(argv[i], "-extractmodels"))
		{
			g_extract_dmodels = true;
		}
		else if (!stricmp(argv[i], "-explodetpages"))
		{
			g_explode_tpages = true;
		}
		else if (!stricmp(argv[i], "-overmap"))
		{
			g_export_overmap = true;
			g_overlaymap_width = atoi(argv[i + 1]);
			i++;
		}
		else if (!stricmp(argv[i], "-lev"))
		{
			levName = argv[i + 1];
			i++;
		}
		else if (!stricmp(argv[i], "-dmodel2obj"))
		{
			ConvertDModelFileToOBJ(argv[i + 1], argv[i + 2]);
			return 0;
		}
		else
		{
			MsgWarning("Unknown command line parameter '%s'\n", argv[i]);
			PrintCommandLineArguments();
			return 0;
		}
	}

	if (levName.length() == 0)
	{
		PrintCommandLineArguments();
		return 0;
	}

	ProcessLevFile(levName.c_str());

	return 0;
}