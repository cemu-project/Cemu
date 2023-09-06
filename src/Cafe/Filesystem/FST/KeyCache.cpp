#include <wx/msgdlg.h>
#include <mutex>
#include <gui/helpers/wxHelpers.h>

#include "config/ActiveSettings.h"
#include "util/crypto/aes128.h"
#include "Common/FileStream.h"
#include "util/helpers/StringHelpers.h"

std::mutex mtxKeyCache;

struct KeyCacheEntry
{
	uint8 aes128key[16];
};

std::vector<KeyCacheEntry> g_keyCache;

bool strishex(std::string_view str)
{
	for(size_t i=0; i<str.size(); i++)
	{
		char c = str[i];
		if( (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F') )
			continue;
		return false;
	}
	return true;
}

/*
 * Returns AES-128 key from the key cache
 * nullptr is returned if index >= max_keys
 */
uint8* KeyCache_GetAES128(sint32 index)
{
	if( index < 0 || index >= (sint32)g_keyCache.size())
		return nullptr;
	KeyCacheEntry* keyCacheEntry = &g_keyCache[index];
	return keyCacheEntry->aes128key;
}

void KeyCache_AddKey128(uint8* key)
{
	KeyCacheEntry newEntry = {0};
	memcpy(newEntry.aes128key, key, 16);
	g_keyCache.emplace_back(newEntry);
}

bool sKeyCachePrepared = false;

void KeyCache_Prepare()
{
	mtxKeyCache.lock();
	if (sKeyCachePrepared)
	{
		mtxKeyCache.unlock();
		return;
	}
	sKeyCachePrepared = true;
	g_keyCache.clear();
	// load keys
	auto keysPath = ActiveSettings::GetUserDataPath("keys.txt");
	FileStream* fs_keys = FileStream::openFile2(keysPath);
	if( !fs_keys )
	{
		fs_keys = FileStream::createFile2(keysPath);
		if(fs_keys)
		{
			fs_keys->writeString("# this file contains keys needed for decryption of disc file system data (WUD/WUX)\r\n");
			fs_keys->writeString("# 1 key per line, any text after a '#' character is considered a comment\r\n");
			fs_keys->writeString("# the emulator will automatically pick the right key\r\n");
			fs_keys->writeString("541b9889519b27d363cd21604b97c67a # example key (can be deleted)\r\n");
			delete fs_keys;
		}
		else
		{
			wxMessageBox(_("Unable to create file keys.txt\nThis can happen if Cemu does not have write permission to its own directory, the disk is full or if anti-virus software is blocking Cemu."), _("Error"), wxOK | wxCENTRE | wxICON_ERROR);
		}
		mtxKeyCache.unlock();
		return;
	}
	sint32 lineNumber = 0;
	std::string line;
	while( fs_keys->readLine(line) )
	{
		lineNumber++;
		// truncate anything after '#' or ';'
		for(size_t i=0; i<line.size(); i++)
		{
			if(line[i] == '#' || line[i] == ';' )
			{
				line.resize(i);
				break;
			}
		}
		// remove whitespaces and other common formatting characters
		auto itr = line.begin();
		while (itr != line.end())
		{
			char c = *itr;
			if (c == ' ' || c == '\t' || c == '-' || c == '_')
				itr = line.erase(itr);
			else
				itr++;
		}
		if (line.empty())
			continue;
		if( strishex(line) == false )
		{
			auto errorMsg = formatWxString(_("Error in keys.txt at line {}"), lineNumber);
			wxMessageBox(errorMsg, _("Error"), wxOK | wxCENTRE | wxICON_ERROR);
			continue;
		}
		if(line.size() == 32 )
		{
			// 128-bit key
			uint8 keyData128[16];
			StringHelpers::ParseHexString(line, keyData128, 16);
			KeyCache_AddKey128(keyData128);
		}
		else
		{
			// invalid key length
		}
	}
	delete fs_keys;
	mtxKeyCache.unlock();
}