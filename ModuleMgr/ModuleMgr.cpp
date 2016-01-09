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
#include "ModuleMgr.h"
#include "ModuleMgrEventListener.h"
#include "cocos2d.h"

#include <curl/curl.h>
#include <curl/easy.h>
#include <stdio.h>

#include "unzip.h"
#include "FileUtilsExtension.h"

#if (CC_PLATFORM_ANDROID == CC_TARGET_PLATFORM)
#include <jni.h>
#include "platform/android/jni/JniHelper.h"
#include <android/log.h>
#endif

NS_CC_EXT_BEGIN

#define TEMP_MANIFEST_FILENAME  "project.manifest.temp"
#define MANIFEST_FILENAME       "project.manifest"

#define BUFFER_SIZE    8192
#define MAX_FILENAME   512

#define DEFAULT_CONNECTION_TIMEOUT 8

const std::string ModuleMgr::MANIFEST_ID = "@manifest";
const std::string ModuleMgr::BATCH_UPDATE_ID = "@batch_update";

// Implementation of ModuleMgr

ModuleMgr::ModuleMgr(const std::string &remoteManifestUrl, const std::string& storagePath)
: _updateState(State::UNCHECKED)
, _waitToUpdate(false)
, _totalToDownload(0)
, _totalWaitToDownload(0)
, _percent(0)
, _percentByFile(0)
, _storagePath("")
, _assets(nullptr)
, _remoteManifestUrl(remoteManifestUrl)
, _remoteManifest(nullptr)

{
    // Init variables
    _eventDispatcher = Director::getInstance()->getEventDispatcher();
    std::string pointer = StringUtils::format("%p", this);
    _eventName = ModuleMgrEventListener::LISTENER_ID + pointer;
    _fileUtils = FileUtils::getInstance();
    _updateState = State::UNCHECKED;

    _downloader = std::make_shared<ModuleDownloader>();
    _downloader->setConnectionTimeout(DEFAULT_CONNECTION_TIMEOUT);
    _downloader->setErrorCallback(std::bind(&ModuleMgr::onError, this, std::placeholders::_1));
    _downloader->setProgressCallback(std::bind(&ModuleMgr::onProgress,
                                         this,
                                         std::placeholders::_1,
                                         std::placeholders::_2,
                                         std::placeholders::_3,
                                         std::placeholders::_4));
    _downloader->setSuccessCallback(std::bind(&ModuleMgr::onSuccess, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    setStoragePath(storagePath);
    
    // Init remote manifest for future usage
    _remoteManifest = new ModuleManifest();
    _remoteManifestPath = storagePath + MANIFEST_FILENAME;
    FileUtils::getInstance()->removeFile(_remoteManifestPath);
}

ModuleMgr::~ModuleMgr()
{
    _downloader->setErrorCallback(nullptr);
    _downloader->setSuccessCallback(nullptr);
    _downloader->setProgressCallback(nullptr);
    
    CC_SAFE_RELEASE(_remoteManifest);
}

ModuleMgr* ModuleMgr::create(const std::string &remoteManifestUrl, const std::string &storagePath)
{
    ModuleMgr* ret = new ModuleMgr(remoteManifestUrl, storagePath);
    if (ret)
    {
        ret->autorelease();
    }
    else
    {
        CC_SAFE_DELETE(ret);
    }
    return ret;
}

std::string ModuleMgr::basename(const std::string& path)
{
    size_t found = path.find_last_of("/\\");
    
    if (std::string::npos != found)
    {
        return path.substr(0, found);
    }
    else
    {
        return path;
    }
}

std::string ModuleMgr::get(const std::string& key) const
{
    auto it = _assets->find(key);
    if (it != _assets->cend()) {
        return _storagePath + it->second.path;
    }
    else return "";
}

const ModuleManifest* ModuleMgr::getRemoteManifest() const
{
    return _remoteManifest;
}

const std::string& ModuleMgr::getStoragePath() const
{
    return _storagePath;
}

void ModuleMgr::setStoragePath(const std::string& storagePath)
{
    if (_storagePath.size() > 0)
        _fileUtils->removeDirectory(_storagePath);

    _storagePath = storagePath;
    adjustPath(_storagePath);
    _fileUtils->createDirectory(_storagePath);
}

void ModuleMgr::adjustPath(std::string &path)
{
    if (path.size() > 0 && path[path.size() - 1] != '/')
    {
        path.append("/");
    }
}

bool ModuleMgr::decompress(const std::string &zip)
{
    // Find root path for zip file
    size_t pos = zip.find_last_of("/\\");
    if (pos == std::string::npos)
    {
        CCLOG("ModuleMgr : no root path specified for zip file %s\n", zip.c_str());
        return false;
    }
    const std::string rootPath = zip.substr(0, pos+1);
    
    // Open the zip file
    unzFile zipfile = unzOpen(zip.c_str());
    if (! zipfile)
    {
        CCLOG("ModuleMgr : can not open downloaded zip file %s\n", zip.c_str());
        return false;
    }
    
    // Get info about the zip file
    unz_global_info global_info;
    if (unzGetGlobalInfo(zipfile, &global_info) != UNZ_OK)
    {
        CCLOG("ModuleMgr : can not read file global info of %s\n", zip.c_str());
        unzClose(zipfile);
        return false;
    }
    
    // Buffer to hold data read from the zip file
    char readBuffer[BUFFER_SIZE];
    // Loop to extract all files.
    uLong i;
    for (i = 0; i < global_info.number_entry; ++i)
    {
        // Get info about current file.
        unz_file_info fileInfo;
        char fileName[MAX_FILENAME];
        if (unzGetCurrentFileInfo(zipfile,
                                  &fileInfo,
                                  fileName,
                                  MAX_FILENAME,
                                  NULL,
                                  0,
                                  NULL,
                                  0) != UNZ_OK)
        {
            CCLOG("ModuleMgr : can not read compressed file info\n");
            unzClose(zipfile);
            return false;
        }
        const std::string fullPath = rootPath + fileName;
        CCLOG("ModuleMgr : extract file %s\n", fullPath.c_str());
        
        // Check if this entry is a directory or a file.
        const size_t filenameLength = strlen(fileName);
        if (fileName[filenameLength-1] == '/')
        {
            //There are not directory entry in some case.
            //So we need to create directory when decompressing file entry
            if ( !_fileUtils->createDirectory(basename(fullPath)) )
            {
                // Failed to create directory
                CCLOG("ModuleMgr : can not create directory %s\n", fullPath.c_str());
                unzClose(zipfile);
                return false;
            }
        }
        else
        {
            // Entry is a file, so extract it.
            // Open current file.
            if (unzOpenCurrentFile(zipfile) != UNZ_OK)
            {
                CCLOG("ModuleMgr : can not extract file %s\n", fileName);
                unzClose(zipfile);
                return false;
            }
            
            if ( !_fileUtils->createDirectory(basename(fullPath)) )
            {
                // Failed to create directory
                CCLOG("ModuleMgr : can not create directory %s\n", fullPath.c_str());
                unzClose(zipfile);
                return false;
            }
            
            // Create a file to store current file.
            FILE *out = fopen(fullPath.c_str(), "wb");
            if (!out)
            {
                CCLOG("ModuleMgr : can not create decompress destination file %s\n", fullPath.c_str());
                unzCloseCurrentFile(zipfile);
                unzClose(zipfile);
                return false;
            }
            
            // Write current file content to destinate file.
            int error = UNZ_OK;
            do
            {
                error = unzReadCurrentFile(zipfile, readBuffer, BUFFER_SIZE);
                if (error < 0)
                {
                    CCLOG("ModuleMgr : can not read zip file %s, error code is %d\n", fileName, error);
                    fclose(out);
                    unzCloseCurrentFile(zipfile);
                    unzClose(zipfile);
                    return false;
                }
                
                if (error > 0)
                {
                    fwrite(readBuffer, error, 1, out);
                }
            } while(error > 0);
            
            fclose(out);
        }
        
        unzCloseCurrentFile(zipfile);
        
        // Goto next entry listed in the zip file.
        if ((i+1) < global_info.number_entry)
        {
            if (unzGoToNextFile(zipfile) != UNZ_OK)
            {
                CCLOG("ModuleMgr : can not read next file for decompressing\n");
                unzClose(zipfile);
                return false;
            }
        }
    }
    
    unzClose(zipfile);
    return true;
}

void ModuleMgr::decompressDownloadedZip()
{
    // Decompress all compressed files
    for (auto it = _compressedFiles.begin(); it != _compressedFiles.end(); ++it) {
        std::string zipfile = *it;
        if (!decompress(zipfile))
        {
            dispatchUpdateEvent(ModuleMgrEvent::EventCode::ERROR_DECOMPRESS, "", "Unable to decompress file " + zipfile);
        }
        _fileUtils->removeFile(zipfile);
    }
    _compressedFiles.clear();
}

void ModuleMgr::dispatchUpdateEvent(ModuleMgrEvent::EventCode code, const std::string &assetId/* = ""*/, const std::string &message/* = ""*/, int curle_code/* = CURLE_OK*/, int curlm_code/* = CURLM_OK*/)
{
    ModuleMgrEvent event(_eventName, this, code, _percent, _percentByFile, assetId, message, curle_code, curlm_code);
    _eventDispatcher->dispatchEvent(&event);
}

ModuleMgr::State ModuleMgr::getState() const
{
    return _updateState;
}

void ModuleMgr::downloadManifest()
{
    if (_updateState != State::PREDOWNLOAD_MANIFEST) {
        return;
    }
    
    if (_remoteManifestUrl.size() > 0)
    {
        _updateState = State::DOWNLOADING_MANIFEST;
        // Download version file asynchronously
        _downloader->downloadAsync(_remoteManifestUrl, _remoteManifestPath, MANIFEST_ID);
    }
    // No manifest file found
    else
    {
        CCLOG("ModuleMgr : No manifest file found, check update failed\n");
        dispatchUpdateEvent(ModuleMgrEvent::EventCode::ERROR_DOWNLOAD_MANIFEST);
        _updateState = State::UNCHECKED;
    }
}

void ModuleMgr::parseManifest()
{
    if (_updateState != State::MANIFEST_LOADED)
        return;

    _remoteManifest->parse(_remoteManifestPath);

    if (!_remoteManifest->isLoaded())
    {
        CCLOG("ModuleMgr : Error parsing manifest file\n");
        dispatchUpdateEvent(ModuleMgrEvent::EventCode::ERROR_PARSE_MANIFEST);
        _updateState = State::UNCHECKED;
    }
    else
    {
//        _updateState = State::NEED_UPDATE;
//        dispatchUpdateEvent(ModuleMgrEvent::EventCode::NEW_VERSION_FOUND);
        
        startUpdate();
    }
}

void ModuleMgr::startUpdate()
{
    // Clean up before update
    _failedUnits.clear();
    _downloadUnits.clear();
    _compressedFiles.clear();
    _totalWaitToDownload = _totalToDownload = 0;
    _percent = _percentByFile = _sizeCollected = _totalSize = 0;
    _downloadedSize.clear();
    _totalEnabled = false;
    
    std::unordered_map<std::string, ModuleManifest::AssetDiff> diff_map = _remoteManifest->genDiff();
    if (diff_map.size() == 0)
    {
        _updateState = State::UP_TO_DATE;
        // Rename temporary manifest to valid manifest
        _fileUtils->removeFile(_remoteManifestPath);
        dispatchUpdateEvent(ModuleMgrEvent::EventCode::ALREADY_UP_TO_DATE);
    }
    else
    {
        if (false == _waitToUpdate) {
            _updateState = State::NEED_UPDATE;
            dispatchUpdateEvent(ModuleMgrEvent::EventCode::NEW_VERSION_FOUND);
            return;
        }
        
        _updateState = State::UPDATING;
        // Generate download units for all assets that need to be updated or added
        std::string packageUrl = _remoteManifest->getPackageUrl();
        for (auto it = diff_map.begin(); it != diff_map.end(); ++it)
        {
            ModuleManifest::AssetDiff diff = it->second;
            
            if (diff.type == ModuleManifest::DiffType::DELETED)
            {
                _fileUtils->removeFile(_storagePath + diff.asset.path);
            }
            else
            {
                std::string path = diff.asset.path;
                // Create path
                _fileUtils->createDirectory(basename(_storagePath + path));
                
                ModuleDownloader::DownloadUnit unit;
                unit.customId = it->first;
                unit.srcUrl = packageUrl + path;
                unit.storagePath = _storagePath + path;
                unit.resumeDownload = false;
                _downloadUnits.emplace(unit.customId, unit);
            }
        }
        // Set other assets' downloadState to SUCCESSED
        auto assets = _remoteManifest->getAssets();
        for (auto it = assets.cbegin(); it != assets.cend(); ++it)
        {
            const std::string &key = it->first;
            auto diffIt = diff_map.find(key);
            if (diffIt == diff_map.end())
            {
                _remoteManifest->setAssetDownloadState(key, ModuleManifest::DownloadState::SUCCESSED);
            }
        }
        
        _totalWaitToDownload = _totalToDownload = (int)_downloadUnits.size();
        _downloader->batchDownloadAsync(_downloadUnits, BATCH_UPDATE_ID);
        
        std::string msg = StringUtils::format("Start to update %d files from remote package.", _totalToDownload);
        dispatchUpdateEvent(ModuleMgrEvent::EventCode::UPDATE_PROGRESSION, "", msg);
    }

    _waitToUpdate = false;
}

void ModuleMgr::updateSucceed()
{
    // Every thing is correctly downloaded, do the following
    // 1. rename temporary manifest to valid manifest
    _fileUtils->renameFile(_storagePath, TEMP_MANIFEST_FILENAME, MANIFEST_FILENAME);

    // 4. decompress all compressed files
    decompressDownloadedZip();
    // 5. Set update state
    _updateState = State::UP_TO_DATE;
    // 6. Notify finished event
    dispatchUpdateEvent(ModuleMgrEvent::EventCode::UPDATE_FINISHED);
}

void ModuleMgr::checkUpdate()
{
    switch (_updateState) {
        case State::UNCHECKED:
        case State::PREDOWNLOAD_MANIFEST:
        {
            _updateState = State::PREDOWNLOAD_MANIFEST;
            downloadManifest();
        }
            break;
        case State::UP_TO_DATE:
        {
            dispatchUpdateEvent(ModuleMgrEvent::EventCode::ALREADY_UP_TO_DATE);
        }
            break;
        case State::FAIL_TO_UPDATE:
        case State::NEED_UPDATE:
        {
            dispatchUpdateEvent(ModuleMgrEvent::EventCode::NEW_VERSION_FOUND);
        }
            break;
        default:
            break;
    }
}

void ModuleMgr::update()
{
    _waitToUpdate = true;

    switch (_updateState) {
        case State::UNCHECKED:
        {
            _updateState = State::PREDOWNLOAD_MANIFEST;
        }
        case State::PREDOWNLOAD_MANIFEST:
        {
            downloadManifest();
        }
            break;
        case State::MANIFEST_LOADED:
        {
            parseManifest();
        }
            break;
        case State::FAIL_TO_UPDATE:
        case State::NEED_UPDATE:
        {
            // Manifest not loaded yet
            if (!_remoteManifest->isLoaded())
            {
                _waitToUpdate = true;
                _updateState = State::PREDOWNLOAD_MANIFEST;
                downloadManifest();
            }
            else
            {
                startUpdate();
            }
        }
            break;
        case State::UP_TO_DATE:
        case State::UPDATING:
            _waitToUpdate = false;
            break;
        default:
            break;
    }
}

void ModuleMgr::updateAssets(const ModuleDownloader::DownloadUnits& assets)
{
    if (_updateState != State::UPDATING && _remoteManifest->isLoaded())
    {
        int size = (int)(assets.size());
        if (size > 0)
        {
            _updateState = State::UPDATING;
            _downloadUnits.clear();
            _downloadUnits = assets;
            _downloader->batchDownloadAsync(_downloadUnits, BATCH_UPDATE_ID);
        }
        else if (size == 0 && _totalWaitToDownload == 0)
        {
            updateSucceed();
        }
    }
}

const ModuleDownloader::DownloadUnits& ModuleMgr::getFailedAssets() const
{
    return _failedUnits;
}

void ModuleMgr::downloadFailedAssets()
{
    CCLOG("ModuleMgr : Start update %lu failed assets.\n", _failedUnits.size());
    updateAssets(_failedUnits);
}


void ModuleMgr::onError(const ModuleDownloader::Error &error)
{
    // Skip version error occured
    if (error.customId == MANIFEST_ID)
    {
        dispatchUpdateEvent(ModuleMgrEvent::EventCode::ERROR_DOWNLOAD_MANIFEST, error.customId, error.message, error.curle_code, error.curlm_code);
    }
    else
    {
        auto unitIt = _downloadUnits.find(error.customId);
        // Found unit and add it to failed units
        if (unitIt != _downloadUnits.end())
        {
            ModuleDownloader::DownloadUnit unit = unitIt->second;
            _failedUnits.emplace(unit.customId, unit);
        }
        dispatchUpdateEvent(ModuleMgrEvent::EventCode::ERROR_UPDATING, error.customId, error.message, error.curle_code, error.curlm_code);
    }
}

void ModuleMgr::onProgress(double total, double downloaded, const std::string &url, const std::string &customId)
{
    if (customId == MANIFEST_ID)
    {
        _percent = 100 * (total - downloaded) / total;
        // Notify progression event
        dispatchUpdateEvent(ModuleMgrEvent::EventCode::UPDATE_PROGRESSION, customId);
        return;
    }
    else
    {
        // Calcul total downloaded
        bool found = false;
        double totalDownloaded = 0;
        for (auto it = _downloadedSize.begin(); it != _downloadedSize.end(); ++it)
        {
            if (it->first == customId)
            {
                it->second = downloaded;
                found = true;
            }
            totalDownloaded += it->second;
        }
        // Collect information if not registed
        if (!found)
        {
            // Set download state to DOWNLOADING, this will run only once in the download process
            _remoteManifest->setAssetDownloadState(customId, ModuleManifest::DownloadState::DOWNLOADING);
            // Register the download size information
            _downloadedSize.emplace(customId, downloaded);
            _totalSize += total;
            _sizeCollected++;
            // All collected, enable total size
            if (_sizeCollected == _totalToDownload)
            {
                _totalEnabled = true;
            }
        }
        
        if (_totalEnabled && _updateState == State::UPDATING)
        {
            float currentPercent = 100 * totalDownloaded / _totalSize;
            // Notify at integer level change
            if ((int)currentPercent != (int)_percent) {
                _percent = currentPercent;
                // Notify progression event
                dispatchUpdateEvent(ModuleMgrEvent::EventCode::UPDATE_PROGRESSION, "");
            }
        }
    }
}

void ModuleMgr::onSuccess(const std::string &srcUrl, const std::string &storagePath, const std::string &customId)
{
    CCLOG("ModuleMgr::onSuccess: %s %s", customId.c_str(), storagePath.c_str());
    if (customId == MANIFEST_ID)
    {
        _updateState = State::MANIFEST_LOADED;
        parseManifest();
    }
    else if (customId == BATCH_UPDATE_ID)
    {
        // Finished with error check
        if (_failedUnits.size() > 0 || _totalWaitToDownload > 0)
        {
            // Save current download manifest information for resuming
            //_tempManifest->saveToFile(_tempManifestPath);
            
            decompressDownloadedZip();
            
            _updateState = State::FAIL_TO_UPDATE;
            dispatchUpdateEvent(ModuleMgrEvent::EventCode::UPDATE_FAILED);
        }
        else
        {
            updateSucceed();
        }
    }
    else
    {
        auto assets = _remoteManifest->getAssets();
        auto assetIt = assets.find(customId);
        if (assetIt != assets.end())
        {
            // Set download state to SUCCESSED
            _remoteManifest->setAssetDownloadState(customId, ModuleManifest::DownloadState::SUCCESSED);
            
            // Add file to need decompress list
            if (assetIt->second.compressed) {
                _compressedFiles.push_back(storagePath);
            }
        }
        
        auto unitIt = _downloadUnits.find(customId);
        if (unitIt != _downloadUnits.end())
        {
            // Reduce count only when unit found in _downloadUnits
            _totalWaitToDownload--;
            
            _percentByFile = 100 * (float)(_totalToDownload - _totalWaitToDownload) / _totalToDownload;
            // Notify progression event
            dispatchUpdateEvent(ModuleMgrEvent::EventCode::UPDATE_PROGRESSION, "");
        }
        // Notify asset updated event
        dispatchUpdateEvent(ModuleMgrEvent::EventCode::ASSET_UPDATED, customId);
        
        unitIt = _failedUnits.find(customId);
        // Found unit and delete it
        if (unitIt != _failedUnits.end())
        {
            // Remove from failed units list
            _failedUnits.erase(unitIt);
        }
    }
}

void ModuleMgr::destroyDownloadedVersion()
{
    _fileUtils->removeFile(_remoteManifestPath);
}

void ModuleMgr::exportZipedSrc(const std::string &zip, const std::string &desPath)
{
    cocos2d::log("ModuleMgr::exportZipedSrc: %s, desPath: %s \n", zip.c_str(), desPath.c_str());
    std::string ver_str = FileUtils::getInstance()->getStringFromFile(desPath + "src/ver");
    auto properties = FileUtils::getInstance()->getValueMapFromFile("proj.plist");
    auto it = properties.find("ControlVersion");
    if (it != properties.end()) {
        auto v = it->second;
        if (v.asString() == ver_str) {
            CCLOG("ModuleMgr::exportZipedSrc : ver %s matches app src, copy exit \n", ver_str.c_str());
#if (COCOS2D_DEBUG == 0)
            return;
#endif
        }
    }
    
//    FileUtils::getInstance()->removeDirectory(desPath + "src");
    FileUtilsExtension::delete_file(desPath + "src");
    
#if (CC_PLATFORM_ANDROID == CC_TARGET_PLATFORM)

    cocos2d::JniMethodInfo minfo;
    if (cocos2d::JniHelper::getStaticMethodInfo(minfo,
                                       "org/cocos2dx/lua/AppActivity",
                                       "extractSrcZip",
                                       "(Ljava/lang/String;Ljava/lang/String;)V"))
    {
        jobject para1 = minfo.env->NewStringUTF(zip.c_str());
        jobject para2 = minfo.env->NewStringUTF((desPath + "src.zip").c_str());
        minfo.env->CallStaticVoidMethod(minfo.classID, minfo.methodID, para1, para2);
        minfo.env->DeleteLocalRef(para1);
        minfo.env->DeleteLocalRef(para2);
    }
    
    std::string fzipPath = desPath + "src.zip";
#elif (CC_PLATFORM_IOS == CC_TARGET_PLATFORM)
    std::string fzipPath = zip;
#endif
    
    // Find root path for zip file
    // Open the zip file
    unzFile zipfile = unzOpen(fzipPath.c_str());
    if (! zipfile)
    {
        CCLOG("ModuleMgr::exportZipedSrc : can not open zipped src file %s\n", fzipPath.c_str());
        return;
    }
    
    // Get info about the zip file
    unz_global_info global_info;
    if (unzGetGlobalInfo(zipfile, &global_info) != UNZ_OK)
    {
        CCLOG("ModuleMgr::exportZipedSrc : can not read file global info of %s\n", zip.c_str());
        unzClose(zipfile);
        return;
    }
    
    // Buffer to hold data read from the zip file
    char readBuffer[BUFFER_SIZE];
    // Loop to extract all files.
    uLong i;
    for (i = 0; i < global_info.number_entry; ++i)
    {
        // Get info about current file.
        unz_file_info fileInfo;
        char fileName[MAX_FILENAME];
        if (unzGetCurrentFileInfo(zipfile,
                                  &fileInfo,
                                  fileName,
                                  MAX_FILENAME,
                                  NULL,
                                  0,
                                  NULL,
                                  0) != UNZ_OK)
        {
            CCLOG("ModuleMgr::exportZipedSrc : can not read compressed file info\n");
            unzClose(zipfile);
            return;
        }
        
        const std::string fullPath = desPath + fileName;
        CCLOG("ModuleMgr::exportZipedSrc : extract file %s\n", fullPath.c_str());
        
        // Check if this entry is a directory or a file.
        const size_t filenameLength = strlen(fileName);
        if (fileName[filenameLength-1] == '/')
        {
            //There are not directory entry in some case.
            //So we need to create directory when decompressing file entry
            if ( !FileUtils::getInstance()->createDirectory(basename(fullPath)) )
            {
                // Failed to create directory
                CCLOG("ModuleMgr::exportZipedSrc : can not create directory %s\n", fullPath.c_str());
                unzClose(zipfile);
                return ;
            }
        }
        else
        {
            // Entry is a file, so extract it.
            // Open current file.
            if (unzOpenCurrentFile(zipfile) != UNZ_OK)
            {
                CCLOG("ModuleMgr::exportZipedSrc : can not extract file %s\n", fileName);
                unzClose(zipfile);
                return;
            }
            
            if ( !FileUtils::getInstance()->createDirectory(basename(fullPath)) )
            {
                // Failed to create directory
                CCLOG("ModuleMgr::exportZipedSrc : can not create directory %s\n", fullPath.c_str());
                unzClose(zipfile);
                return;
            }
            
            // Create a file to store current file.
            FILE *out = fopen(fullPath.c_str(), "wb");
            if (!out)
            {
                CCLOG("ModuleMgr::exportZipedSrc : can not create decompress destination file %s\n", fullPath.c_str());
                unzCloseCurrentFile(zipfile);
                unzClose(zipfile);
                return;
            }
            
            // Write current file content to destinate file.
            int error = UNZ_OK;
            do
            {
                error = unzReadCurrentFile(zipfile, readBuffer, BUFFER_SIZE);
                if (error < 0)
                {
                    CCLOG("ModuleMgr::exportZipedSrc : can not read zip file %s, error code is %d\n", fileName, error);
                    fclose(out);
                    unzCloseCurrentFile(zipfile);
                    unzClose(zipfile);
                    return;
                }
                
                if (error > 0)
                {
                    fwrite(readBuffer, error, 1, out);
                }
            } while(error > 0);
            
            fclose(out);
        }
        
        unzCloseCurrentFile(zipfile);
        
        ModuleManifest::setFileTime(fullPath, ModuleManifest::dosDateToTime(fileInfo.dosDate));
        
        // Goto next entry listed in the zip file.
        if ((i+1) < global_info.number_entry)
        {
            if (unzGoToNextFile(zipfile) != UNZ_OK)
            {
                CCLOG("ModuleMgr::exportZipedSrc : can not read next file for decompressing\n");
                unzClose(zipfile);
                return;
            }
        }
    }
    
    unzClose(zipfile);
    
    FileUtils::getInstance()->removeFile(desPath + "src.zip");
    
    auto itr = properties.find("ControlVersion");
    if (itr != properties.end()) {
        auto v = itr->second;
        
        std::string ver_content = v.asString();
        std::string ver_des = desPath + "src/ver";
        FileUtils::getInstance()->removeFile(ver_des);
        
        FILE *out = fopen(ver_des.c_str(), "wb");
        if (!out)
        {
            CCLOG("ModuleMgr::exportZipedSrc : can not create ver file %s\n", ver_des.c_str());
            return;
        }
        
        // Write current file content to destinate file.
        fwrite(ver_content.c_str(), ver_content.length(), 1, out);
        fclose(out);
    }
    
    
    
    return;
}

NS_CC_EXT_END
