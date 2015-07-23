
void HelloWorld::selfUpdateInit()
{
    std::string storagePath = FileUtils::getInstance()->getWritablePath();
    std::string srcPath = FileUtils::getInstance()->fullPathForFilename("res/src.zip");
    
    ModuleMgr::exportZipedSrc(srcPath, storagePath);
    
    _am = cocos2d::extension::ModuleMgr::create(HostMgr::getHostByKey(kHOST_Manifest), storagePath);
    // As the process is asynchronies, you need to retain the assets manager to make sure it won't be released before the process is ended.
    _am->retain();
}

void HelloWorld::selfUpdate()
{
    static ModuleMgrEventListener* _amListener = ModuleMgrEventListener::create(_am, [this](ModuleMgrEvent* event){
        static int failCount = 0;
        switch (event->getEventCode())
        {
            case ModuleMgrEvent::EventCode::UPDATE_PROGRESSION:
            {
                std::string assetId = event->getAssetId();
                float totalPercent = event->getPercentByFile();
                
                if (ModuleMgr::MANIFEST_ID != assetId) {
                    std::string str = StringUtils::format("总计: %s 已更新 %.2f%%", _totalLen.c_str(), totalPercent);
                    
                    auto node = this->getChildByTag(1);
                    if (node) {
                        auto l = dynamic_cast<Label*>(node);
                        if (l) {
                            l->setString(str);
                        }
                    }
                }
            }
                break;
            case ModuleMgrEvent::EventCode::ERROR_DOWNLOAD_MANIFEST:
            case ModuleMgrEvent::EventCode::ERROR_PARSE_MANIFEST:
            {
                CCLOG("Fail to download manifest file, update skipped.");
                this->selfUpdateEnded();
            }
                break;
            case ModuleMgrEvent::EventCode::ALREADY_UP_TO_DATE:
            case ModuleMgrEvent::EventCode::UPDATE_FINISHED:
            {
                CCLOG("Update finished. %s", event->getMessage().c_str());
                this->selfUpdateEnded();
            }
                break;
            case ModuleMgrEvent::EventCode::UPDATE_FAILED:
            {
                CCLOG("Update failed. %s", event->getMessage().c_str());
                
                failCount ++;
                if (failCount < 5)
                {
                    _am->downloadFailedAssets();
                }
                else
                {
                    CCLOG("Reach maximum fail count, exit update process");
                    failCount = 0;
                    this->selfUpdateEnded();
                }
            }
                break;
            case ModuleMgrEvent::EventCode::ERROR_UPDATING:
            {
                CCLOG("ERROR_UPDATING Asset %s : %s", event->getAssetId().c_str(), event->getMessage().c_str());
            }
                break;
            case ModuleMgrEvent::EventCode::ERROR_DECOMPRESS:
            {
                CCLOG("ERROR_DECOMPRESS %s", event->getMessage().c_str());
            }
                break;
            case ModuleMgrEvent::EventCode::ASSET_UPDATED:
            {
                CCLOG("ASSET_UPDATED %s", event->getMessage().c_str());
            }
                break;
            case ModuleMgrEvent::EventCode::NEW_VERSION_FOUND:
            {
                CCLOG("NEW_VERSION_FOUND %s", event->getMessage().c_str());
                
                if (_am->getRemoteManifest()->isForceUpdate()) {
                    _am->update();
                }
                else {
                    int64_t diffLen = _am->getRemoteManifest()->diffLength();
                    
                    if (diffLen == 0) {
                        this->selfUpdateEnded();
                        return;
                    }
                    
                    double bytes = (double)diffLen;
                    std::string pszUnits[] = { ("B"), ("KB"), ("MB"), ("GB"), ("TB") };
                    
                    int64_t cUnits = sizeof(pszUnits) / sizeof(pszUnits[0]);
                    int64_t cIter = 0;
                    
                    // move from bytes to KB, to MB, to GB and so on diving by 1024
                    while (bytes >= 1024 && cIter < (cUnits - 1))
                    {
                        bytes /= 1024;
                        cIter++;
                    }
                    
                    std::string bsize = StringUtils::format("%.2f %s", bytes, pszUnits[cIter].c_str());
                    _totalLen = std::string(bsize);
                    std::string msg = "需更新大小 " + bsize;
                    
                    auto systemlay = CAlert::create();
                    this->addChild(systemlay, 1000, 12345);
                    auto label2 = LabelTTF::create(msg, "Arial", 24);
                    label2->setColor(Color3B::BLACK);
                    label2->setPosition(550,320);
                    systemlay->addChild(label2, 7);
                    
                    systemlay->clickEventOK = [=] (cocos2d::Node* node) {
                        _am->update();
                    };
                    
                    systemlay->clickEventCancel = [=] (cocos2d::Node* node) {
                        this->selfUpdateEnded();
                    };
                }
            }
                break;
            default:
                break;
        }
    });
    
    Director::getInstance()->getEventDispatcher()->addEventListenerWithFixedPriority(_amListener, 1);
    
    _am->checkUpdate();
    
}

void HelloWorld::selfUpdateEnded()
{
    Director::getInstance()->replaceScene(GameScene::createScene());
}