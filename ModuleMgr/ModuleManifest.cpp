/****************************************************************************
 Copyright (c) 2014 cocos2d-x.org
 
 http://www.cocos2d-x.org
 
 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 ****************************************************************************/

#include "ModuleManifest.h"
#include "json/filestream.h"
#include "json/prettywriter.h"
#include "json/stringbuffer.h"

#include <fstream>
#include <time.h>
#include <utime.h>
#include <sys/stat.h>

#define KEY_VERSION             "version"
#define KEY_FORCE_UPDATE        "forceUpdate"
#define KEY_PACKAGE_URL         "packageUrl"
#define KEY_ModuleManifest_URL  "remoteManifestUrl"
#define KEY_VERSION_URL         "remoteVersionUrl"
#define KEY_GROUP_VERSIONS      "groupVersions"
#define KEY_ENGINE_VERSION      "engineVersion"
#define KEY_ASSETS              "assets"
#define KEY_COMPRESSED_FILES    "compressedFiles"
#define KEY_SEARCH_PATHS        "searchPaths"

#define KEY_PATH                "path"
#define KEY_LEN                 "len"
#define KEY_TIMESTAMP           "timestamp"
#define KEY_CONTENT             "content"
#define KEY_GROUP               "group"
#define KEY_COMPRESSED          "compressed"
#define KEY_COMPRESSED_FILE     "compressedFile"
#define KEY_DOWNLOAD_STATE      "downloadState"

NS_CC_EXT_BEGIN

ModuleManifest::ModuleManifest(const std::string& ModuleManifestUrl/* = ""*/)
: _loaded(false)
, _ModuleManifestRoot("")
, _remoteModuleManifestUrl("")
, _forceUpdate(false)
, _diffLen(0)
{
    // Init variables
    _fileUtils = FileUtils::getInstance();
    if (ModuleManifestUrl.size() > 0)
        parse(ModuleManifestUrl);
}

void ModuleManifest::parse(const std::string& ModuleManifestUrl)
{
    clear();
	std::string content;
	if (_fileUtils->isFileExist(ModuleManifestUrl))
	{
		// Load file content
		content = _fileUtils->getStringFromFile(ModuleManifestUrl);

		if (content.size() == 0)
		{
			CCLOG("Fail to retrieve local file content: %s\n", ModuleManifestUrl.c_str());
		}
		else
		{
			// Parse file with rapid json
			_json.Parse<0>(content.c_str());
			// Print error
			if (_json.HasParseError()) {
			size_t offset = _json.GetErrorOffset();
			if (offset > 0)
			offset--;
			std::string errorSnippet = content.substr(offset, 10);
			CCLOG("File parse error %s at <%s>\n", _json.GetParseError(), errorSnippet.c_str());
			}
		}
	}
	
    if (_json.IsObject())
    {
        // Register the local ModuleManifest root
        size_t found = ModuleManifestUrl.find_last_of("/\\");
        if (found != std::string::npos)
        {
            _ModuleManifestRoot = ModuleManifestUrl.substr(0, found+1);
        }
        loadModuleManifest(_json);
    }
}

bool ModuleManifest::isLoaded() const
{
    return _loaded;
}

bool ModuleManifest::isForceUpdate() const
{
#if COCOS2D_DEBUG
    return false;
#endif
    return _forceUpdate;
}

int64_t ModuleManifest::diffLength() const
{
    return _diffLen;
}

void ModuleManifest::setDiffLength(int64_t len)
{
    _diffLen = len;
}

bool ModuleManifest::versionEquals(const ModuleManifest *b) const
{
    // Check ModuleManifest version
    return false;
}

int64_t ModuleManifest::getFileLen(const std::string& filePath) {
    std::ifstream in(filePath, std::ifstream::ate | std::ifstream::binary);
    int64_t len = in.tellg();
    return len;
}

int64_t ModuleManifest::getFileTime(const std::string& filePath) {
    struct stat attr;
    stat(filePath.c_str(), &attr);
    return attr.st_mtime;
}

int64_t ModuleManifest::dosDateToTime(unsigned long ulDosDate) {
    const unsigned long uDate = (unsigned long)(ulDosDate>>16);
    struct tm ptm;
    
    ptm.tm_mday = (unsigned int)(uDate&0x1f) ;
    ptm.tm_mon =  (unsigned int)((((uDate)&0x1E0)/0x20)-1) ;
    ptm.tm_year = (unsigned int)(((uDate&0x0FE00)/0x0200)+1980) - 1900 ;
    
    ptm.tm_hour = (unsigned int) ((ulDosDate &0xF800)/0x800);
    ptm.tm_min =  (unsigned int) ((ulDosDate&0x7E0)/0x20) ;
    ptm.tm_sec =  (unsigned int) (2*(ulDosDate&0x1f)) ;
    
    auto time = mktime(&ptm);
    
    return time;
}

int64_t ModuleManifest::setFileTime(const std::string& filePath, long long timestamp) {
    struct stat attr;
    
    stat(filePath.c_str(), &attr);
    
    struct utimbuf new_times;
    new_times.actime = attr.st_atime; /* keep atime unchanged */
    new_times.modtime = timestamp;    /* set mtime to current time */
    utime(filePath.c_str(), &new_times);
    
    return 0;
}

std::unordered_map<std::string, ModuleManifest::AssetDiff> ModuleManifest::genDiff()
{
    std::unordered_map<std::string, AssetDiff> diff_map;
    
    std::string key;
    Asset valueA;
    std::unordered_map<std::string, Asset>::const_iterator valueIt, it;
    
    int64_t diffLen = 0;
    for (it = _assets.begin(); it != _assets.end(); ++it)
    {
        key = it->first;
        valueA = it->second;
        
        if (valueA.compressed) {
            for (auto bit : valueA.content) {
                auto fLen = getFileLen(FileUtils::getInstance()->getWritablePath() + bit.second.path);
                if (fLen != bit.second.len) {
                    auto fTime = getFileTime(FileUtils::getInstance()->getWritablePath() + bit.second.path) + 60;
                    if (fTime < bit.second.timestamp) {
                        AssetDiff diff;
                        diff.asset = valueA;
                        diff.type = DiffType::MODIFIED;
                        diff_map.emplace(key, diff);
                        diffLen += valueA.len;
                        break;
                    }
                }
            }
        }
        else {
            auto fLen = getFileLen(FileUtils::getInstance()->getWritablePath() + valueA.path);
            if (fLen != valueA.len) {
                auto fTime = getFileTime(FileUtils::getInstance()->getWritablePath() + valueA.path) + 60;
                if (fTime < valueA.timestamp) {
                    AssetDiff diff;
                    diff.asset = valueA;
                    diff.type = DiffType::MODIFIED;
                    diff_map.emplace(key, diff);
                    diffLen += valueA.len;
                }
            }
        }
    }
    
    CCLOG("ModuleManifest::genDiff diff_map");
    for (auto it : diff_map) {
        CCLOG("ModuleManifest::genDiff %s", it.first.c_str());
    }
    
    setDiffLength(diffLen);
    return diff_map;
}

void ModuleManifest::genResumeAssetsList(ModuleDownloader::DownloadUnits *units) const
{
    for (auto it = _assets.begin(); it != _assets.end(); ++it)
    {
        Asset asset = it->second;
        
        if (asset.downloadState != DownloadState::SUCCESSED)
        {
            ModuleDownloader::DownloadUnit unit;
            unit.customId = it->first;
            unit.srcUrl = _packageUrl + asset.path;
            unit.storagePath = _ModuleManifestRoot + asset.path;
            if (asset.downloadState == DownloadState::DOWNLOADING)
            {
                unit.resumeDownload = true;
            }
            else
            {
                unit.resumeDownload = false;
            }
            units->emplace(unit.customId, unit);
        }
    }
}


void ModuleManifest::prependSearchPaths()
{
    std::vector<std::string> searchPaths = FileUtils::getInstance()->getSearchPaths();
    std::vector<std::string>::iterator iter = searchPaths.begin();
    searchPaths.insert(iter, _ModuleManifestRoot);
    
    for (int i = (int)_searchPaths.size()-1; i >= 0; i--)
    {
        std::string path = _searchPaths[i];
        if (path.size() > 0 && path[path.size() - 1] != '/')
            path.append("/");
        path = _ModuleManifestRoot + path;
        iter = searchPaths.begin();
        searchPaths.insert(iter, path);
    }
    FileUtils::getInstance()->setSearchPaths(searchPaths);
}

const std::string& ModuleManifest::getPackageUrl() const
{
    return _packageUrl;
}

const std::string& ModuleManifest::getModuleManifestFileUrl() const
{
    return _remoteModuleManifestUrl;
}

const std::unordered_map<std::string, ModuleManifest::Asset>& ModuleManifest::getAssets() const
{
    return _assets;
}

void ModuleManifest::setAssetDownloadState(const std::string &key, const ModuleManifest::DownloadState &state)
{
    auto valueIt = _assets.find(key);
    if (valueIt != _assets.end())
    {
        valueIt->second.downloadState = state;
        
        // Update json object
        if(_json.IsObject())
        {
            if ( _json.HasMember(KEY_ASSETS) )
            {
                rapidjson::Value &assets = _json[KEY_ASSETS];
                if (assets.IsObject())
                {
                    for (rapidjson::Value::MemberIterator itr = assets.MemberonBegin(); itr != assets.MemberonEnd(); ++itr)
                    {
                        std::string jkey = itr->name.GetString();
                        if (jkey == key) {
                            rapidjson::Value &entry = itr->value;
                            rapidjson::Value &value = entry[KEY_DOWNLOAD_STATE];
                            if (value.IsInt())
                            {
                                value.SetInt((int)state);
                            }
                            else
                            {
                                entry.AddMember<int>(KEY_DOWNLOAD_STATE, (int)state, _json.GetAllocator());
                            }
                        }
                    }
                }
            }
        }
    }
}

void ModuleManifest::clear()
{
    if (_loaded)
    {
        _remoteModuleManifestUrl = "";
        _assets.clear();
        _searchPaths.clear();
        _loaded = false;
    }
}

ModuleManifest::Asset ModuleManifest::parseAsset(const std::string &path, const rapidjson::Value &json)
{
    Asset asset;
    asset.path = path;
    
    if ( json.HasMember(KEY_LEN) )
    {
        if ( json[KEY_LEN].IsInt64() ) {
            asset.len = json[KEY_LEN].GetInt64();
        }
        else if ( json[KEY_LEN].IsInt() ) {
            asset.len = json[KEY_LEN].GetInt();
        }
    }
    else asset.len = 0;

    if ( json.HasMember(KEY_TIMESTAMP) )
    {
        if ( json[KEY_TIMESTAMP].IsInt64() ) {
            asset.timestamp = json[KEY_TIMESTAMP].GetInt64();
        }
        else if ( json[KEY_TIMESTAMP].IsInt() ) {
            asset.timestamp = json[KEY_TIMESTAMP].GetInt();
        }
    }
    else asset.timestamp = 0;
    
    if ( json.HasMember(KEY_PATH) && json[KEY_PATH].IsString() )
    {
        asset.path = json[KEY_PATH].GetString();
    }
    
    if ( json.HasMember(KEY_COMPRESSED) && json[KEY_COMPRESSED].IsBool() )
    {
        asset.compressed = json[KEY_COMPRESSED].GetBool();
    }
    else asset.compressed = false;
    
    if ( json.HasMember(KEY_DOWNLOAD_STATE) && json[KEY_DOWNLOAD_STATE].IsInt() )
    {
        asset.downloadState = (DownloadState)(json[KEY_DOWNLOAD_STATE].GetInt());
    }
    else asset.downloadState = DownloadState::UNSTARTED;
    
    asset.content.clear();
    const rapidjson::Value& content = json[KEY_CONTENT];
    if (content.IsObject())
    {
        for (rapidjson::Value::ConstMemberIterator itr = content.MemberonBegin(); itr != content.MemberonEnd(); ++itr)
        {
            std::string key = itr->name.GetString();
            
            SubAsset subasset;
            subasset.path = key;
            
            if ( itr->value.HasMember(KEY_LEN) )
            {
                if ( itr->value[KEY_LEN].IsInt64() ) {
                    subasset.len = itr->value[KEY_LEN].GetInt64();
                }
                else if ( itr->value[KEY_LEN].IsInt() ) {
                    subasset.len = itr->value[KEY_LEN].GetInt();
                }
            }
            else subasset.len = 0;
            
            if ( itr->value.HasMember(KEY_TIMESTAMP) )
            {
                if ( itr->value[KEY_TIMESTAMP].IsInt64() ) {
                    subasset.timestamp = itr->value[KEY_TIMESTAMP].GetInt64();
                }
                else if ( itr->value[KEY_TIMESTAMP].IsInt() ) {
                    subasset.timestamp = itr->value[KEY_TIMESTAMP].GetInt();
                }
            }
            else subasset.timestamp = 0;
            
            if ( itr->value.HasMember(KEY_PATH) && itr->value[KEY_PATH].IsString() )
            {
                subasset.path = itr->value[KEY_PATH].GetString();
            }
            
            asset.content.emplace(key, subasset);
        }
    }
    
    return asset;
}

void ModuleManifest::loadModuleManifest(const rapidjson::Document &json)
{
    if ( json.HasMember(KEY_ModuleManifest_URL) && json[KEY_ModuleManifest_URL].IsString() )
    {
        _remoteModuleManifestUrl = json[KEY_ModuleManifest_URL].GetString();
    }
    
    if ( json.HasMember(KEY_FORCE_UPDATE) && json[KEY_FORCE_UPDATE].IsTrue() )
    {
        _forceUpdate = true;
    }
    else {
        _forceUpdate = false;
    }
    
    // Retrieve package url
    if ( json.HasMember(KEY_PACKAGE_URL) && json[KEY_PACKAGE_URL].IsString() )
    {
        _packageUrl = json[KEY_PACKAGE_URL].GetString();
        // Append automatically "/"
        if (_packageUrl.size() > 0 && _packageUrl[_packageUrl.size() - 1] != '/')
        {
            _packageUrl.append("/");
        }
    }
    
    // Retrieve all assets
    if ( json.HasMember(KEY_ASSETS) )
    {
        const rapidjson::Value& assets = json[KEY_ASSETS];
        if (assets.IsObject())
        {
            for (rapidjson::Value::ConstMemberIterator itr = assets.MemberonBegin(); itr != assets.MemberonEnd(); ++itr)
            {
                std::string key = itr->name.GetString();
                Asset asset = parseAsset(key, itr->value);
                _assets.emplace(key, asset);
            }
        }
    }
    
    // Retrieve all search paths
    if ( json.HasMember(KEY_SEARCH_PATHS) )
    {
        const rapidjson::Value& paths = json[KEY_SEARCH_PATHS];
        if (paths.IsArray())
        {
            for (rapidjson::SizeType i = 0; i < paths.Size(); ++i)
            {
                if (paths[i].IsString()) {
                    _searchPaths.push_back(paths[i].GetString());
                }
            }
        }
    }
    
    _loaded = true;
}

void ModuleManifest::saveToFile(const std::string &filepath)
{
    rapidjson::StringBuffer buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
    if (!_json.IsNull()) {
        _json.Accept(writer);
        
        std::ofstream output(filepath, std::ofstream::out);
        if(!output.bad())
            output << buffer.GetString() << std::endl;
    }
}

NS_CC_EXT_END