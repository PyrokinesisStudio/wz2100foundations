/*
	This file is part of Warzone 2100.
	Copyright (C) 1999-2004  Eidos Interactive
	Copyright (C) 2005-2013  Warzone 2100 Project

	Warzone 2100 is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	Warzone 2100 is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with Warzone 2100; if not, write to the Free Software
	Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
*/
/**
 * @file configuration.cpp
 * Saves your favourite options.
 *
 */

#include "lib/framework/wzapp.h"
#include "lib/framework/wzconfig.h"
#include "lib/framework/input.h"
#include "lib/netplay/netplay.h"
#include "lib/sound/mixer.h"
#include "lib/ivis_opengl/screen.h"
#include "lib/framework/opengl.h"

#include "advvis.h"
#include "ai.h"
#include "component.h"
#include "configuration.h"
#include "difficulty.h"
#include "display3d.h"
#include "hci.h"
#include "multiint.h"
#include "multiplay.h"
#include "radar.h"
#include "seqdisp.h"
#include "texture.h"
#include "warzoneconfig.h"	// renderMode

#include "substrings.h"

#include <sys/stat.h>

// ////////////////////////////////////////////////////////////////////////////

#define DEFAULTSCROLL	1000
#define MASTERSERVERPORT	9990
#define GAMESERVERPORT		2100

static const char *fileName = "config";

class IniReader
{
public:
	enum Status { STATUS_NOERROR, STATUS_ERROR };
	
	struct ValueObject
	{
		const char *String;

		bool toBool(void) const
		{
			if (!this->String) return false;
			
			if (SubStrings.CaseCompare("true", this->String)) return true;
			else if (SubStrings.CaseCompare("false", this->String)) return false;

			return std::stoi(String);
			
		}
		
		operator const char *(void) const { return this->String; }
		
		int64_t toInt(void) const { return this->String ? atoll(this->String) : 0; }
		long double toDouble(void) const { return this->String ? strtold(this->String, nullptr) : 0.0l; }
	};
private:
	std::map<std::string, std::string> Lines;
	std::string FileName;
	Status OpenStatus;
public:
	IniReader(const std::string &FilePath);
	Status status(void) const { return this->OpenStatus; }
	bool contains(const char *String) const { return this->Lines.count(String); }
	bool contains(const std::string &String) const { return this->Lines.count(String); }
	
	
	ValueObject value(const char *String, const bool DefaultValue)
	{
		if (!this->Lines.count(String)) this->Lines[String] = DefaultValue ? "true" : "false";

		return ValueObject { this->Lines.at(String).c_str() };
	}
	ValueObject value(const char *String, const int DefaultValue)
	{
		if (!this->Lines.count(String)) this->Lines[String] = std::to_string(DefaultValue);
	
		return ValueObject { this->Lines.at(String).c_str() };
	}
	
	ValueObject value(const char *String, const char *DefaultValue)
	{
		if (!this->Lines.count(String)) this->Lines[String] = DefaultValue;
		
		return ValueObject { this->Lines.at(String).c_str() };
	}
	ValueObject value(const char *String) const
	{
		if (!this->Lines.count(String)) return ValueObject();
		
		ValueObject RetVal = { this->Lines.at(String).c_str() };
		
		return RetVal;
	}
	ValueObject value(const std::string &String) const { return this->value(String.c_str()); }
	
	const char *fileName(void) const { return this->FileName.c_str(); }
	
	void setValue(const char *Key, const char *String) { this->Lines[Key] = String; }
	void setValue(const char *Key, const short Integer) { this->Lines[Key] = std::to_string(Integer); }
	void setValue(const char *Key, const unsigned short Integer) { this->Lines[Key] = std::to_string(Integer); }
	void setValue(const char *Key, const int Integer) { this->Lines[Key] = std::to_string(Integer); }
	void setValue(const char *Key, const unsigned Integer) { this->Lines[Key] = std::to_string(Integer); }
	void setValue(const char *Key, const long Integer) { this->Lines[Key] = std::to_string(Integer); }
	void setValue(const char *Key, const unsigned long Integer) { this->Lines[Key] = std::to_string(Integer); }
	void setValue(const char *Key, const long long Integer) { this->Lines[Key] = std::to_string(Integer); }
	void setValue(const char *Key, const unsigned long long Integer) { this->Lines[Key] = std::to_string(Integer); }
	void setValue(const char *Key, const long double Floaty) { this->Lines[Key] = std::to_string(Floaty); }
	void setValue(const char *Key, const bool Value) { this->Lines[Key] = Value ? "true" : "false"; }
	bool sync(void) const;
};


IniReader::IniReader(const std::string &FilePath_) : FileName(FilePath_), OpenStatus()
{
	const char *FilePath = FilePath_.c_str();
	
	struct stat FileStat{};
	
	if (stat(FilePath, &FileStat) != 0)
	{
		this->OpenStatus = STATUS_ERROR;
		return;
	}
	
	FILE *Desc = fopen(FilePath, "rb");
	
	if (!Desc)
	{
		this->OpenStatus = STATUS_ERROR;
		return;
	}
	
	char *Buf = new char[FileStat.st_size + 1]();
	
	fread(Buf, 1, FileStat.st_size, Desc);
	fclose(Desc);
	
	const char *Ptr = Buf;
	
	char LineBuf[2048];
	
	while (SubStrings.Line.GetLine(LineBuf, sizeof LineBuf, &Ptr))
	{
		SubStrings.StripLeadingChars(LineBuf, " \t");
		
		if (!*LineBuf || *LineBuf == '[' || *LineBuf == '#') continue;
		
		if (!strchr(LineBuf, '='))
		{
			debug(LOG_WARNING, "Bad data in config file");
			continue;
		}
		
		char Key[sizeof LineBuf];
		char Value[sizeof LineBuf];
		
		SubStrings.Split(Key, Value, "=", LineBuf, SPLIT_NOKEEP);
		
		//Get rid of any whitespace that shouldn't be there.
		SubStrings.StripTrailingChars(Key, " \t");
		SubStrings.StripLeadingChars(Value, " \t");
		SubStrings.StripTrailingChars(Value, " \t");
		
		this->Lines[Key] = Value;
	}
	
	delete[] Buf;
}

bool IniReader::sync(void) const
{
	FILE *Desc = fopen(this->FileName.c_str(), "w"); //NOT "wb", cuz we want it to do the newline conversions on Windows.
	
	if (!Desc) return false;
	
	for (auto Iter = this->Lines.begin(); Iter != this->Lines.end(); ++Iter)
	{
		const std::string Line = std::string(Iter->first) + "=" + Iter->second + "\n";
		fwrite(Line.c_str(), 1, Line.length(), Desc);
	}
	
	fclose(Desc);
	return true;
}


// ////////////////////////////////////////////////////////////////////////////
bool loadConfig()
{
	IniReader ini(std::string(PHYSFS_getWriteDir()) + PHYSFS_getDirSeparator() + fileName);
	if (ini.status() != IniReader::STATUS_NOERROR)
	{
		debug(LOG_WARNING, "Could not open configuration file \"%s\"", fileName);
	}
	debug(LOG_WZ, "Reading configuration from %s", ini.fileName());
	if (ini.contains("voicevol"))
	{
		sound_SetUIVolume(ini.value("voicevol").toDouble() / 100.0);
	}
	if (ini.contains("fxvol"))
	{
		sound_SetEffectsVolume(ini.value("fxvol").toDouble() / 100.0);
	}
	if (ini.contains("cdvol"))
	{
		sound_SetMusicVolume(ini.value("cdvol").toDouble() / 100.0);
	}
	if (ini.contains("music_enabled"))
	{
		war_SetMusicEnabled(ini.value("music_enabled").toBool());
	}
	if (ini.contains("language"))
	{
		setLanguage(ini.value("language"));
	}
	if (ini.contains("nomousewarp"))
	{
		setMouseWarp(ini.value("nomousewarp").toBool());
	}
	if (ini.contains("notexturecompression"))
	{
		wz_texture_compression = GL_RGBA;
	}
	showFPS = ini.value("showFPS", false).toBool();
	scroll_speed_accel = ini.value("scroll", DEFAULTSCROLL).toInt();
	setShakeStatus(ini.value("shake", false).toBool());
	setDrawShadows(ini.value("shadows", true).toBool());
	war_setSoundEnabled(ini.value("sound", true).toBool());
	setInvertMouseStatus(ini.value("mouseflip", true).toBool());
	setRightClickOrders(ini.value("RightClickOrders", false).toBool());
	setMiddleClickRotate(ini.value("MiddleClickRotate", false).toBool());
	rotateRadar = ini.value("rotateRadar", true).toBool();
	war_SetPauseOnFocusLoss(ini.value("PauseOnFocusLoss", false).toBool());
	NETsetMasterserverName(ini.value("masterserver_name", "lobby.wz2100.net"));
	iV_font(ini.value("fontname", "DejaVu Sans"),
	        ini.value("fontface", "Book"),
	        ini.value("fontfacebold", "Bold"));
	NETsetMasterserverPort(ini.value("masterserver_port", MASTERSERVERPORT).toInt());
	NETsetGameserverPort(ini.value("gameserver_port", GAMESERVERPORT).toInt());
	war_SetFMVmode((FMV_MODE)ini.value("FMVmode", FMV_FULLSCREEN).toInt());
	war_setScanlineMode((SCANLINE_MODE)ini.value("scanlines", SCANLINES_OFF).toInt());
	seq_SetSubtitles(ini.value("subtitles", true).toBool());
	setDifficultyLevel((DIFFICULTY_LEVEL)ini.value("difficulty", DL_NORMAL).toInt());
	war_SetSPcolor(ini.value("colour", 0).toInt());	// default is green (0)
	war_setMPcolour(ini.value("colourMP", -1).toInt());  // default is random (-1)
	sstrcpy(game.name, ini.value("gameName", _("My Game")));
	sstrcpy(sPlayer, ini.value("playerName", _("Player")));

	// Set a default map to prevent hosting games without a map.
	sstrcpy(game.map, "Sk-Rush");
	game.hash.setZero();
	game.maxPlayers = 4;

	game.power = ini.value("power", LEV_MED).toInt();
	game.base = ini.value("base", CAMP_BASE).toInt();
	game.alliance = ini.value("alliance", NO_ALLIANCES).toInt();
	memset(&ingame.phrases, 0, sizeof(ingame.phrases));
	for (int i = 1; i < 5; i++)
	{
		std::string Key = std::string("phrase") + std::to_string(i);
		
		if (ini.contains(Key))
		{
			sstrcpy(ingame.phrases[i], ini.value(Key));
		}
	}
	bEnemyAllyRadarColor = ini.value("radarObjectMode").toBool();
	radarDrawMode = (RADAR_DRAW_MODE)ini.value("radarTerrainMode", RADAR_MODE_DEFAULT).toInt();
	radarDrawMode = (RADAR_DRAW_MODE)MIN(NUM_RADAR_MODES - 1, radarDrawMode); // restrict to allowed values
	if (ini.contains("textureSize"))
	{
		setTextureSize(ini.value("textureSize").toInt());
	}
	else
	{
		setTextureSize(512);	//default size for textures
	}
	NetPlay.isUPNP = ini.value("UPnP", true).toBool();
	if (ini.contains("FSAA"))
	{
		war_setFSAA(ini.value("FSAA").toInt());
	}
	if (ini.contains("shaders"))
	{
		war_SetShaders(ini.value("shaders").toInt());
	}
	// Leave this to false, some system will fail and they can't see the system popup dialog!
	war_setFullscreen(ini.value("fullscreen", false).toBool());
	war_SetTrapCursor(ini.value("trapCursor", false).toBool());
	// this should be enabled on all systems by default
	war_SetVsync(ini.value("vsync", true).toBool());
	// 640x480 is minimum that we will support
	int width = ini.value("width", 640).toInt();
	int height = ini.value("height", 480).toInt();
	if (width < 640 || height < 480)	// sanity check
	{
		width = 640;
		height = 480;
	}
	pie_SetVideoBufferWidth(width);
	pie_SetVideoBufferHeight(height);
	war_SetWidth(width);
	war_SetHeight(height);

	if (ini.contains("bpp"))
	{
		pie_SetVideoBufferDepth(ini.value("bpp").toInt());
	}
	return true;
}

// ////////////////////////////////////////////////////////////////////////////
bool saveConfig()
{
	IniReader ini(std::string(PHYSFS_getWriteDir()) + PHYSFS_getDirSeparator() + fileName);

	if (ini.status() != IniReader::STATUS_NOERROR)
	{
		debug(LOG_WARNING, "Could not open configuration file \"%s\"", fileName);
	}
	debug(LOG_WZ, "Writing prefs to registry \"%s\"", ini.fileName());

	// //////////////////////////
	// voicevol, fxvol and cdvol
	ini.setValue("voicevol", (int)(sound_GetUIVolume() * 100.0));
	ini.setValue("fxvol", (int)(sound_GetEffectsVolume() * 100.0));
	ini.setValue("cdvol", (int)(sound_GetMusicVolume() * 100.0));
	ini.setValue("music_enabled", war_GetMusicEnabled());
	ini.setValue("width", war_GetWidth());
	ini.setValue("height", war_GetHeight());
	ini.setValue("bpp", pie_GetVideoBufferDepth());

	const bool FSValue = war_getFullscreen();
	printf("Fullscreen value at saveConfig() is %s\n", FSValue ? "true" : "false");
	
	ini.setValue("fullscreen", FSValue);
	ini.setValue("language", getLanguage());
	// dont save out the cheat mode.
	if (getDifficultyLevel() != DL_KILLER && getDifficultyLevel() != DL_TOUGH)
	{
		ini.setValue("difficulty", getDifficultyLevel());		// level
	}
	ini.setValue("showFPS", (SDWORD)showFPS);
	ini.setValue("scroll", (SDWORD)scroll_speed_accel);		// scroll
	ini.setValue("shake", (SDWORD)(getShakeStatus()));		// screenshake
	ini.setValue("mouseflip", (SDWORD)(getInvertMouseStatus()));	// flipmouse
	ini.setValue("nomousewarp", (SDWORD)getMouseWarp()); 		// mouse warp
	ini.setValue("RightClickOrders", (SDWORD)(getRightClickOrders()));
	ini.setValue("MiddleClickRotate", (SDWORD)(getMiddleClickRotate()));
	ini.setValue("showFPS", (SDWORD)showFPS);
	ini.setValue("shadows", (SDWORD)(getDrawShadows()));	// shadows
	ini.setValue("sound", (SDWORD)war_getSoundEnabled());
	ini.setValue("FMVmode", (SDWORD)(war_GetFMVmode()));		// sequences
	ini.setValue("scanlines", (SDWORD)war_getScanlineMode());
	ini.setValue("subtitles", (SDWORD)(seq_GetSubtitles()));		// subtitles
	ini.setValue("radarObjectMode", (SDWORD)bEnemyAllyRadarColor);   // enemy/allies radar view
	ini.setValue("radarTerrainMode", (SDWORD)radarDrawMode);
	ini.setValue("trapCursor", war_GetTrapCursor());
	ini.setValue("vsync", war_GetVsync());
	ini.setValue("shaders", war_GetShaders());
	ini.setValue("textureSize", getTextureSize());
	ini.setValue("FSAA", war_getFSAA());
	ini.setValue("UPnP", (SDWORD)NetPlay.isUPNP);
	ini.setValue("rotateRadar", rotateRadar);
	ini.setValue("PauseOnFocusLoss", war_GetPauseOnFocusLoss());
	ini.setValue("masterserver_name", NETgetMasterserverName());
	ini.setValue("masterserver_port", NETgetMasterserverPort());
	ini.setValue("gameserver_port", NETgetGameserverPort());
	if (!bMultiPlayer)
	{
		ini.setValue("colour", getPlayerColour(0));			// favourite colour.
	}
	else
	{
		if (NetPlay.isHost && ingame.localJoiningInProgress)
		{
			if (bMultiPlayer && NetPlay.bComms)
			{
				ini.setValue("gameName", game.name);			//  last hosted game
			}
			ini.setValue("mapName", game.map);				//  map name
			ini.setValue("mapHash", game.hash.toString().c_str());          //  map hash
			ini.setValue("maxPlayers", game.maxPlayers);		// maxPlayers
			ini.setValue("power", game.power);				// power
			ini.setValue("base", game.base);				// size of base
			ini.setValue("alliance", game.alliance);		// allow alliances
		}
		ini.setValue("playerName", (char *)sPlayer);		// player name
	}
	ini.setValue("colourMP", war_getMPcolour());
	ini.sync();
	return true;
}

// Saves and loads the relevant part of the config files for MP games
// Ensures that others' games don't change our own configuration settings
bool reloadMPConfig(void)
{
	IniReader ini(std::string(PHYSFS_getWriteDir()) + PHYSFS_getDirSeparator() + fileName);

	if (ini.status() != IniReader::STATUS_NOERROR)
	{
		debug(LOG_ERROR, "Could not open configuration file \"%s\"", fileName);
		return false;
	}
	debug(LOG_WZ, "Reloading prefs prefs to registry");

	// If we're in-game, we already have our own configuration set, so no need to reload it.
	if (NetPlay.isHost && !ingame.localJoiningInProgress)
	{
		if (bMultiPlayer && !NetPlay.bComms)
		{
			// one-player skirmish mode sets game name to "One Player Skirmish", so
			// reset the name
			if (ini.contains("gameName"))
			{
				sstrcpy(game.name, ini.value("gameName"));
			}
		}
		return true;
	}

	// If we're host and not in-game, we can safely save our settings and return.
	if (NetPlay.isHost && ingame.localJoiningInProgress)
	{
		if (bMultiPlayer && NetPlay.bComms)
		{
			ini.setValue("gameName", game.name);			//  last hosted game
		}
		else
		{
			// One-player skirmish mode sets game name to "One Player Skirmish", so
			// reset the name
			if (ini.contains("gameName"))
			{
				sstrcpy(game.name, ini.value("gameName"));
			}
		}

		// Set a default map to prevent hosting games without a map.
		sstrcpy(game.map, "Sk-Rush");
		game.hash.setZero();
		game.maxPlayers = 4;

		ini.setValue("power", game.power);				// power
		ini.setValue("base", game.base);				// size of base
		ini.setValue("alliance", game.alliance);		// allow alliances
		return true;
	}

	// We're not host, so let's get rid of the host's game settings and restore our own.

	// game name
	if (ini.contains("gameName"))
	{
		sstrcpy(game.name, ini.value("gameName"));
	}

	// Set a default map to prevent hosting games without a map.
	sstrcpy(game.map, "Sk-Rush");
	game.hash.setZero();
	game.maxPlayers = 4;

	game.power = ini.value("power", LEV_MED).toInt();
	game.base = ini.value("base", CAMP_BASE).toInt();
	game.alliance = ini.value("alliance", NO_ALLIANCES).toInt();

	return true;
}

void closeConfig()
{
}
