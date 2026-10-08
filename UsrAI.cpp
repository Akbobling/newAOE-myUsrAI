#include "UsrAI.h"
#include "RuntimeConfig.h"
#include "config.h"
#include <set>
#include <iostream>
#include <unordered_map>
#include <list>
#include <cstdlib>
#include <algorithm>
#include <climits>

// 调试总开关：将 DebugText 替换为空操作以关闭所有既有调试输出，
// 仅保留单独使用 DebugText 的新增调试点（如祭祀 [priDBG]）
#define DBG_OFF(...) ((void)0)

using namespace std;
tagGame tagUsrGame;
ins UsrIns;
/*##########DO NOT MODIFY THE CODE ABOVE##########*/

tagInfo info;
const int MAP_SIZE = 100;
const int MAX_DISTANCE = 40;            // 最大移动距离
int centerX = -1, centerY = -1;
int priestSN = -1;
int centerSN = -1;
int dx[4] = {5,0,-5,0},
    dy[4] = {0,5,0,-5};
int cDir = 0, cMX = 60, cMY = 60;
int gameMap[MAP_SIZE+1][MAP_SIZE+1];
int exploreValueMap[MAP_SIZE+1][MAP_SIZE+1];
int enemyAtkRangeMap[MAP_SIZE+1][MAP_SIZE+1];
int treeCoverMap[MAP_SIZE+1][MAP_SIZE+1];
vector<buildTask> buildTasks;
map<int, int> farmerTask;
map<int, int> farmerState;
int builderSN = -1;
int totalMeat = 200;
double lastkillX = -1, lastkillY = -1;
int granaryX = -1, granaryY = -1;
int nearestResourceX, nearestResourceY;
int needStock = 0;
int marketSN = -1;
bool isUpdatingStage = false;
bool needFixArrowTower = false;

int huntingPhase = 0;
int nearestGazelleSN = -1;
int stockX=0, stockY=0;
int arrowTowerX=-1, arrowTowerY=-1;
int homeX=-1, homeY=-1;
int stockSN = -1;
int stockBuilt = 0;
int HumanControl = 20;   // 农民人口上限（只统计农民，不含军队）：gamePhase1 为 20，gamePhase2 起为 25
int farmNum = 0;
GoldCluster bestCluster;            // 金矿簇选址（goldMining 首次计算；gamePhase2 farming 启动判定读取）
bool goldClusterReady = false;      // 金矿簇是否已成功选定
int farmDx[8] = {-4,-4,-4,0,0,4,4,4};
int farmDy[8] = {-4,0,4,-4,4,-4,0,4};
vector<int> deadMeatResources;
set<int> currentFarmersSN;
set<int> arrowTowerSN;

// ==== 浆果采集 / 资源动态切换状态 ====
vector<int> berryResourceSNs;      // 开局记录的浆果资源 SN
set<int> assignedBerrySNs;         // 已分配给村民的浆果 SN（一对一）
map<int,int> berryAssignment;      // 村民SN -> 浆果SN
set<int> hunterFarmersSN;          // 猎人（开局两人配对狩猎 + resourceSwitch 后一半伐木工加入收尸）
set<int> corpseHelperSNs;          // 由伐木组转入的收尸帮工（resourceSwitch 加入，收尸时均分到各尸体）
set<int> berryFarmersSN;           // 浆果采集村民（新生成村民）
set<int> stoneFarmersSN;           // 采石村民
set<int> repairFarmersSN;          // 箭塔维修村民（14000帧后从采石组二次分配而来）
set<int> goldFarmersSN;            // 采金村民（gamePhase2 统一分配后的主力采集组）
set<int> treeFarmersSN;            // 伐木村民（供应房屋/军队/科技木材；farming 每建一块田临时抽调一人）
bool unifiedAssignDone = false;    // gamePhase2 是否已执行统一分配
bool hunterInitialized = false;    // 开局分工是否已初始化（builder/浆果/待命村民）
int waitingHunterSN = -1;          // 开局待命村民：原地不动，等首个新村民会合后两人一起打猎
int pairingFarmerSN = -1;          // 正在走向待命村民的市镇中心新村民（配对期间不被其它模块抢派）
bool resourceSwitchDone = false;   // 木头够建军营+市场+马厩后切换：伐木组对半分（一半帮收羚羊尸体）
bool repairRepurposeDone = false;  // 需求5：14000帧采石村民二次分配
bool compositeBowDone = false;    // 复合弓科技是否升级成功（ins_ret == ACTION_SUCCESS）
bool wheelDone = false;            // 车轮科技是否升级成功（全局，createArmy1 和 gamePhase2 共用）
bool phase3Active = false;         // 第三阶段反攻是否已开始（priest 据此切换为随军转化模式）
int phase3PriestTargetX = -1;      // 第三阶段祭司跟进落点（块坐标，gamePhase3 每拍更新）
int phase3PriestTargetY = -1;
bool phase3FinalPush = false;      // 第三阶段最终总攻是否已触发（gamePhase3 每拍自
                                   // s_finalPush 同步；总攻前祭司只在地图中心待命）
// 祭祀"最终攻夺"（唯一触发）：方阵兵将敌方箭塔打至50血以下 → 强制转化敌方攻城武器厂
bool finalSiegeCaptureTriggered = false;
int  finalSiegeSN = -1;                    // 目标攻城武器厂 SN
int  finalSiegeX = -1, finalSiegeY = -1;   // 攻城武器厂块坐标（祭司进军/视野判定用）
int  finalSiegeTriggerFrame = -1;          // 触发帧：祭司先原地停留2秒(50帧)再动身，
                                           // 让攻坚部队先接住箭塔仇恨再压进
// 祭祀哨点解锁（2026-10-08规则）：进入最终反攻阶段后，敌方箭塔完全摧毁数量
// 达到3座前，祭祀不得脱离哨点行动（最终攻夺也须等解锁后才触发）
bool priestTowerUnlocked = false;
// 祭祀转化目标保护（2026-10-08）：priestReserveSN 为祭祀当前锁定的转化目标，
// 目标存活且锁定未超时(1800帧=45秒)时，箭塔与家中防守部队不得将其射杀——
// 转化收益(单位归我)远大于击杀；祭祀休整期(20秒)内目标同样受保护，
// 避免"祭祀刚转化完上一个、箭塔把下一个打死了"的转化效率损耗
int priestReserveSN = -1;
int priestReserveFrame = -100000;
map<int, pair<int,int>> farmerFarmPos;   // 耕作村民 SN -> 其被派去建的农田左上角块坐标
bool woodUpgradePending = true;   // 木材加工升级待下发（resourceSwitch 后置位，成功后清除）


int UsrAI::getBuildingSideLen(int type) {
    switch (type) {
        case BUILDING_HOME:
        case BUILDING_ARROWTOWER:
        case BUILDING_DOCK:
            return 2;   // SIZELEN_SMALL
        case BUILDING_STOCK:
        case BUILDING_GRANARY:
        case BUILDING_CENTER:
        case BUILDING_FARM:
        case BUILDING_MARKET:
        case BUILDING_ARMYCAMP:
        case BUILDING_STABLE:
        case BUILDING_RANGE:
        case BUILDING_SIEGE:
        case BUILDING_COLLAGE:
            return 3;   // SIZELEN_MIDDLE
        default:
            return 1;
    }
}

// 资源占格宽度（对齐内核 StaticRes::setAttribute 的 BlockSizeLen）
int UsrAI::getResourceSideLen(int type) {
    switch (type) {
        case RESOURCE_STONE:
        case RESOURCE_GOLD:
        case RESOURCE_FISH:
            return 2;   // SIZELEN_SMALL
        default:
            return 1;
    }
}

void UsrAI::init()
{
    info = getInfo();

    if(priestSN == -1) {
        for(auto& army: info.armies) {
            if(army.Sort == AT_PRIEST) {
                priestSN = army.SN;
                break;
            }
        }
    }

    // 建筑工存活检测：阵亡后立即从在册村民中选任新建筑工，避免建造永久停摆
    bool builderAlive = false;
    for(auto& farmer: info.farmers) {
        if(farmer.SN == builderSN) { builderAlive = true; break; }
    }
    if(!builderAlive) {
        int oldBuilder = builderSN;
        builderSN = -1;
        // 优先选非生产组的空闲村民（不中断伐木/采金/耕作）
        for(auto& farmer: info.farmers) {
            if(treeFarmersSN.count(farmer.SN) || goldFarmersSN.count(farmer.SN)
               || currentFarmersSN.count(farmer.SN) || repairFarmersSN.count(farmer.SN)
               || berryFarmersSN.count(farmer.SN) || hunterFarmersSN.count(farmer.SN)
               || stoneFarmersSN.count(farmer.SN)) continue;
            builderSN = farmer.SN;
            break;
        }
        if(builderSN == -1 && !info.farmers.empty()) {
            builderSN = info.farmers[0].SN;   // 兜底：取第一个在册村民
        }
        if(builderSN != -1) {
            // 新建筑工从原组中移除，避免同帧被 logging/goldMining 等抢派冲突指令
            treeFarmersSN.erase(builderSN);
            goldFarmersSN.erase(builderSN);
            currentFarmersSN.erase(builderSN);
            DebugText("[builderDBG] 旧建筑工 " + to_string(oldBuilder) + " 阵亡，新任 " + to_string(builderSN)
                      + " frame=" + to_string(info.GameFrame));
        }
    }

    // 开局分工（一次性）：builder 之外前 6 人采浆果，最后 1 人原地待命
    // 待命村民等市镇中心生产出首个新村民并走到身边后，两人一起打猎（pairFirstHunters）
    if(!hunterInitialized) {
        int berryCnt = 0;
        for(auto& farmer: info.farmers) {
            if(farmer.SN == builderSN) continue;
            if(berryCnt < 6) {
                berryFarmersSN.insert(farmer.SN);   // 具体浆果目标由 berryCollecting 维护循环分配
                berryCnt++;
            } else if(waitingHunterSN == -1) {
                waitingHunterSN = farmer.SN;
            }
            // 多余村民不标记：由 logging() 统一认领砍树
        }
        hunterInitialized = true;
    }

    int stockNum = 0;
    int fNum = 0;
    for(auto& building: info.buildings) {
        if(building.Type == BUILDING_GRANARY) {
            granaryX = building.BlockDR;
            granaryY = building.BlockUR;
        }
        if(building.Type == BUILDING_ARROWTOWER) {
            arrowTowerSN.insert(building.SN);
        }
        if(building.Type == BUILDING_STOCK) {
            stockSN = building.SN;
            if(building.Blood == building.MaxBlood)
                stockNum ++;
        }
        if(building.Type == BUILDING_CENTER) {
            centerX = building.BlockDR;
            centerY = building.BlockUR;
            centerSN = building.SN;
        }
        if(arrowTowerX == -1 && building.Type == BUILDING_ARROWTOWER) {
            arrowTowerX = building.BlockDR;
            arrowTowerY = building.BlockUR;
        }
        if(homeX == -1 && building.Type == BUILDING_HOME) {
            homeX = building.BlockDR;
            homeY = building.BlockUR;
        }
        if(building.Type == BUILDING_MARKET) {
            marketSN = building.SN;
        }
    }

    if(stockNum >= 2) stockBuilt = 1;

    // 初始化地图为未占用状态
    for (int i = 0; i < MAP_SIZE; i++) {
        for (int j = 0; j < MAP_SIZE; j++) {
            gameMap[i][j] = 0;
            enemyAtkRangeMap[i][j] = 0;
            exploreValueMap[i][j] = 0;
            treeCoverMap[i][j] = 0;
            if (info.theMap != nullptr) {
                const tagTerrain& terrain = (*info.theMap)[i][j];
                int height = terrain.height;
                int type = terrain.type;
                if(type == MAPPATTERN_UNKNOWN) {
                    gameMap[i][j] = -1;
                    exploreValueMap[i][j] = 3;
                } else if(type == MAPPATTERN_OCEAN || height == -1) {
                    gameMap[i][j] = 1;
                    if(type == MAPPATTERN_OCEAN) 
                        exploreValueMap[i][j] = -1;
                }
            }
        }
    }

    int gazelleNum = 0;

    for(auto& resource: info.resources) {
        if(resource.Type == RESOURCE_GAZELLE) {
            gazelleNum ++;
        }
    }

    // 遍历所有资源并在map上标注为已占用
    for (int i = 0; i < info.resources.size(); i++) {
        int rx = info.resources[i].BlockDR;
        int ry = info.resources[i].BlockUR;
        int size = getResourceSideLen(info.resources[i].Type);
        for(int dx = 0; dx < size; dx++) {
            for(int dy = 0; dy < size; dy++) {
                int nx = rx + dx;
                int ny = ry + dy;
                if (nx >= 0 && nx < MAP_SIZE && ny >= 0 && ny < MAP_SIZE) {
                    gameMap[nx][ny] = 1;
                    if(info.resources[i].Type == RESOURCE_GOLD) {
                        exploreValueMap[nx][ny] += 1;
                    }
                    if(info.resources[i].Type == RESOURCE_LION) {
                        exploreValueMap[nx][ny] -= 1;
                    }
                    if(info.resources[i].Type == RESOURCE_TREE) {
                        treeCoverMap[nx][ny] += 1;
                        exploreValueMap[nx][ny] -= 2;
                    }
                    if(info.resources[i].Type == RESOURCE_GAZELLE) {
                        if(gazelleNum <= 3) {
                            exploreValueMap[nx][ny] += 1;
                        } else {
                            exploreValueMap[nx][ny] -= 3;
                        }
                    }
                }
            }
        }
    }

    // 遍历所有建筑并在map上标注为已占用
    needFixArrowTower = false;
    for (int i = 0; i < info.buildings.size(); i++) {
        if(info.buildings[i].Type == BUILDING_ARROWTOWER && info.buildings[i].Blood != info.buildings[i].MaxBlood) {
            needFixArrowTower = true;
        }
        int bx = info.buildings[i].BlockDR;
        int by = info.buildings[i].BlockUR;
        int size = getBuildingSideLen(info.buildings[i].Type);
        for (int dx = 0; dx < size; dx++) {
            for (int dy = 0; dy < size; dy++) {
                int nx = bx + dx;
                int ny = by + dy;
                if (nx >= 0 && nx < MAP_SIZE && ny >= 0 && ny < MAP_SIZE) {
                    gameMap[nx][ny] = 1;
                }
            }
        }
    }
    
    for(auto& enemy: info.enemy_armies) {
        for(int dx = -15; dx <= 15; dx++) {
            for (int dy = -15; dy <= 15; dy++) {
                int nx = enemy.BlockDR + dx;
                int ny = enemy.BlockUR + dy;
                if (nx >= 0 && nx < MAP_SIZE && ny >= 0 && ny < MAP_SIZE) {
                    exploreValueMap[nx][ny] -= 5;
                }
            }
        }
        // 需求6：仅纳入正在进攻祭祀单位的敌人（WorkObjectSN == priestSN）
        if(enemy.WorkObjectSN != priestSN) continue;
        int atkRange = getAttackRange(enemy.Sort) + 2;
        for(int dx = -atkRange; dx <= atkRange; dx++) {
            for (int dy = -atkRange; dy <= atkRange; dy++) {
                int nx = enemy.BlockDR + dx;
                int ny = enemy.BlockUR + dy;
                if (nx >= 0 && nx < MAP_SIZE && ny >= 0 && ny < MAP_SIZE) {
                    enemyAtkRangeMap[nx][ny] = 1;
                }
            }
        }
    }
    
    for (int i=0; i < info.farmers.size(); i++) {
        int fx = info.farmers[i].BlockDR;
        int fy = info.farmers[i].BlockUR;
        if (fx >= 0 && fx < MAP_SIZE && fy >= 0 && fy < MAP_SIZE) {
            gameMap[fx][fy] = 1;
        }
    }
    for(auto& army: info.armies) {
        gameMap[army.BlockDR][army.BlockUR] = 1;
    }
}

bool UsrAI::checkRequiredBuilding()
{
    const int requiredNum = 2;
    int builtNum = 0;
    for(auto& building : info.buildings) {
        if(building.Type == BUILDING_MARKET || building.Type == BUILDING_STABLE || building.Type == BUILDING_RANGE) {
            builtNum++;
        }
    }
    if(builtNum >= requiredNum) return true;
    return false;
}

void UsrAI::updateStage()
{
    static int lastOrderId = -1;
    static int lastOrderTime = 0;

    // 上一帧升级指令结果（ins_ret 仅下一帧有效），读到后复位，便于观察真实失败码
    if(lastOrderId != -1) {
        auto it = info.ins_ret.find(lastOrderId);
        if(it != info.ins_ret.end()) {
            DBG_OFF("BronzeAge upgrade ins_ret code = " + to_string(it->second)
                      + " stage = " + to_string(info.civilizationStage)
                      + " frame = " + to_string(info.GameFrame));
            lastOrderId = -1;
        }
    }

    // 节流：避免每帧重复下发升级指令（重复下发会重置升级进度，导致永远无法完成）
    if(info.GameFrame - lastOrderTime < 100) return;
    lastOrderTime = info.GameFrame;

    if(info.civilizationStage <= CIVILIZATION_TOOLAGE &&
       info.Meat >= BUILDING_CENTER_UPGRADE_BRONZEAGE_FOOD &&
       checkRequiredBuilding()) {
        for(auto& building : info.buildings) {
            // 仅当市镇中心空闲时才下发，避免打断正在进行的训练/升级；也避免与 createFarmer 同帧抢占同一建筑
            if(building.Type == BUILDING_CENTER && building.Project == ACT_NULL) {
                lastOrderId = BuildingAction(building.SN, BUILDING_CENTER_UPGRADE);
                DBG_OFF("Action");
                break;
            }
        }
    }
}

void UsrAI::updateTech()
{
    static int ArrowUnlocked = 1;
    if(ArrowUnlocked && info.Meat >= BUILDING_GRANARY_ARROWTOWER_FOOD) {
        ArrowUnlocked = 0;
        for(auto& building : info.buildings) {
            if(building.Type == BUILDING_GRANARY) {
                BuildingAction(building.SN, BUILDING_GRANARY_ARROWTOWER);
                break;
            }
        }
    }
}

void UsrAI::createFarmer()
{
    // 升级条件已满足时暂停造农民，让市镇中心保持空闲以完成时代升级；
    // 否则 createFarmer（在 updateStage 之后调用）会用 CREATE_FARMER 覆盖本帧的升级指令
    if(info.civilizationStage <= CIVILIZATION_TOOLAGE &&
       info.Meat >= BUILDING_CENTER_UPGRADE_BRONZEAGE_FOOD &&
       checkRequiredBuilding()) {
        return;
    }

    for(auto& building : info.buildings) {
        if(building.Type == BUILDING_CENTER && building.Project == ACT_NULL &&
        info.Meat >= BUILDING_CENTER_CREATEFARMER_FOOD && info.farmers.size() < info.Human_MaxNum && info.farmers.size() < HumanControl) {
            BuildingAction(building.SN, BUILDING_CENTER_CREATEFARMER);
            totalMeat += BUILDING_CENTER_CREATEFARMER_FOOD;
            break;
        }
    }
}

void UsrAI::createArmy()
{
    return;
}

// 兵种攻击距离（块）。近战统一按 1 算，远程按内核 DIS_xxx 配置
int UsrAI::getAttackRange(int armyType)
{
    switch (armyType) {
        case AT_BOWMAN:            return 6;    // 弓箭手
        case AT_COMPOSITE_BOWMAN:  return 8;    // 复合弓兵
        case AT_CHARIOT_ARCHER:    return 8;    // 战车弓兵
        case AT_STONE_THROWER:     return 11;   // 投石车
        case AT_SLINGER:           return 5;    // 投石兵
        default:                   return 1;    // 棍棒兵/战斧兵/侦察骑兵/骑兵/四马战车/阔剑兵/方阵兵等近战
    }
}

void UsrAI::priest()
{
    static int lastOrderX = -1;
    static int lastOrderY = -1;
    static int lastPriestX = -1;
    static int lastPriestY = -1;
    static int lastOrderFrame = - 10000;
    static int returned = 0;
    static vector<pair<int,int>> visitedPoints;
    static bool converting = false;      // 需求7：是否正在转化敌方单位
    static int convertTargetSN = -1;     // 当前转化目标 SN
    static int convertLockCooldown = 0;  // 近战打断后的重新锁定冷却（先脱离威胁再选新目标）
    static int lastPriestBlood = -1;     // 上帧祭祀血量，用于检测受攻击
    const int retryInterval = 50;
    const int radius = 5;

    int priestX = -1, priestY = -1, priestBlood = -1;
    int convertRestMs = 0;   // 内核转化休整剩余（毫秒）：>0 时转化指令被内核拒绝并强制站定
    for(auto& army: info.armies) {
        if(army.Sort == AT_PRIEST) {
            priestX = army.BlockDR;
            priestY = army.BlockUR;
            priestBlood = army.Blood;
            convertRestMs = army.ConvertCooldown;
            break;
        }
    }
    if(priestX == -1) return;

    // 内核转化休整期（转化成功后 PRIEST_REST_TIME=20 秒：Core_List.cpp:2304 拒绝转化指令、
    // :1048 强制站定）——期间继续选目标只会让祭祀被远程点射时纹丝不动。
    // 压制转化重锁（每帧刷新 convertLockCooldown），让休整期走进无目标的保命移动逻辑
    if(convertRestMs > 0) {
        converting = false;
        convertTargetSN = -1;
        convertLockCooldown = info.GameFrame + 25;
    }

    // ===== 祭祀"最终攻夺"（第三阶段触发，优先级最高，忽略当前其它转化指令）=====
    // 触发条件（gamePhase3 判定）：方阵兵将任意敌方箭塔打至50血以下（唯一触发）。
    // 触发后：攻城武器厂为唯一转化目标；未进视野时直接向其进军；
    // 正在转化攻城厂时不重发指令（读条不中断）。
    if(finalSiegeCaptureTriggered) {
        // 触发后先原地停留2秒(50帧)再动身：让攻坚部队先接住箭塔仇恨，祭司再压进
        if(finalSiegeTriggerFrame != -1
           && info.GameFrame - finalSiegeTriggerFrame < 50) {
            return;
        }
        bool siegeAlive = false;
        for(auto& b : info.enemy_buildings) {
            if(b.SN == finalSiegeSN && b.Blood > 0) { siegeAlive = true; break; }
        }
        if(siegeAlive) {
            int cheb = max(abs(finalSiegeX - priestX), abs(finalSiegeY - priestY));
            if(converting && convertTargetSN == finalSiegeSN) {
                return;   // 已在读条转化攻城厂：不重发指令，任凭远程扣血读完
            }
            if(cheb <= VISION_PRIEST) {
                if(convertRestMs <= 0) {           // 内核休整期内转化指令会被拒绝，等待休整结束
                    convertTargetSN = finalSiegeSN;
                    HumanAction(priestSN, finalSiegeSN);
                    converting = true;
                    lastOrderFrame = info.GameFrame;
                }
            } else if(info.GameFrame - lastOrderFrame >= 20) {
                // 未进视野：向攻城厂进军
                HumanMove(priestSN, finalSiegeX * BLOCKSIDELENGTH, finalSiegeY * BLOCKSIDELENGTH);
                lastOrderFrame = info.GameFrame;
            }
            return;   // 触发后常规转化/走位逻辑全部不再介入
        }
        // 攻城厂已不存在（被误拆等）：回落常规逻辑
    }

    //DBG_OFF("lastOrderX = " + to_string(lastOrderX) + ", lastOrderY = " + to_string(lastOrderY));
    //DBG_OFF("manhattan_distance = " + to_string(manhattan_distance(lastOrderX,lastOrderY,priestX,priestY)))
    // 需求7：转化保护——已经开始读条就不要停止当前转化，除非近战兵已经近身
    // 远程攻击扣血可接受，避免持续风筝（站着读完条比边跑边挨远程打更优）
    if(converting) {
        bool targetExists = false;
        bool targetForbidden = false;   // 阔剑兵(不得转化) → 立即中断
        for(auto& enemy : info.enemy_armies) {
            if(enemy.SN == convertTargetSN) {
                targetExists = true;
                targetForbidden = (enemy.Sort == AT_BROADSWORDSMAN);
                break;
            }
        }
        // 不得转化阔剑兵（2026-10-08）：存量读条立即中断解锁换目标
        if(targetForbidden) {
            converting = false;
            convertTargetSN = -1;
            targetExists = false;
            DebugText("[priDBG] forbidden convert target interrupted; frame="
                      + to_string(info.GameFrame));
        }
        // 转化目标也可能是敌方建筑（第三阶段终局转化攻城武器厂）
        if(!targetExists) {
            for(auto& b : info.enemy_buildings) {
                if(b.SN == convertTargetSN && b.Blood > 0) { targetExists = true; break; }
            }
        }
        // 敌人近身（仅统计近战兵种，切比雪夫<=2，即将进入近战攻击范围）时不再站着读条
        // 远程敌人贴近开火属于可接受的扣血，不中断读条；混入远程判定会让读条永远被打断
        bool meleeClosing = false;
        for(auto& enemy : info.enemy_armies) {
            if(getAttackRange(enemy.Sort) <= 1
               && max(abs(enemy.BlockDR - priestX), abs(enemy.BlockUR - priestY)) <= 2) {
                meleeClosing = true;
                break;
            }
        }
        // 目标还在且无近身威胁 → 继续读条，即使被远程打掉血也不中断
        if(targetExists && !meleeClosing) {
            return;
        }
        converting = false;
        // 近身威胁中断时解锁目标并短暂冷却，按新优先级重选
        // （冷却期内走风筝/后撤移动逻辑，防止原地立刻重锁同一目标形成读条重置死循环）
        if(meleeClosing) {
            DebugText("[priDBG] melee closing, unlock convert target; frame="
                      + to_string(info.GameFrame));
            convertTargetSN = -1;
            convertLockCooldown = info.GameFrame + 60;
        }
    }

    // ===== 第三阶段反攻：随军跟进 + 前线转化（覆盖家中防守走位逻辑）=====
    if(phase3Active) {
        const int P3_INTERVAL = 20;
        if(info.GameFrame - lastOrderFrame < P3_INTERVAL) return;

        // 友军 SN 集合（建筑+军队），转化目标要求仇恨在友军身上
        set<int> p3FriendSNs;
        for(auto& b : info.buildings) if(b.Blood > 0) p3FriendSNs.insert(b.SN);
        for(auto& a : info.armies)   if(a.Blood > 0) p3FriendSNs.insert(a.SN);

        // 当前锁定目标是否仍存在（敌军或建筑）
        bool p3TargetAlive = false;
        if(convertTargetSN != -1) {
            for(auto& enemy : info.enemy_armies)
                if(enemy.SN == convertTargetSN && enemy.Blood > 0) { p3TargetAlive = true; break; }
            if(!p3TargetAlive)
                for(auto& b : info.enemy_buildings)
                    if(b.SN == convertTargetSN && b.Blood > 0) { p3TargetAlive = true; break; }
            if(!p3TargetAlive) convertTargetSN = -1;
        }

        // ===== 2026-10-08 新规：第三阶段祭司待命 =====
        // 最终总攻(phase3FinalPush)触发前只待在地图中心待命，不得行动——不选
        // 转化目标、不下转化指令；最终攻夺已触发时除外（那是唯一获胜路径）。
        // 待命期间仅保留保命撤退（被点名/近身/处塔射程内时撤离，见下方移动逻辑）。
        bool p3ActionAllowed = (phase3FinalPush || finalSiegeCaptureTriggered);
        if(!p3ActionAllowed) {
            convertTargetSN = -1;
            converting = false;
        }

        // 三级优先选敌军目标：攻祭司 > 方阵兵 > 其它；必须在视野内且仇恨在友军身上
        // 近战敌人已贴脸(切比雪夫<=3)时不选它读条——读条必被打断，交给后撤移动逻辑处理
        if(p3ActionAllowed && convertTargetSN == -1 && info.GameFrame >= convertLockCooldown) {
            int minDistAtkPriest = INT_MAX, minDistPhalanx = INT_MAX, minDistAny = INT_MAX;
            int atkPriestSN = -1, phalanxSN = -1, anySN = -1;
            for(auto& enemy : info.enemy_armies) {
                if(enemy.Blood <= 0) continue;
                int edx = enemy.BlockDR - priestX, edy = enemy.BlockUR - priestY;
                if(edx*edx + edy*edy > VISION_PRIEST * VISION_PRIEST) continue;
                if(p3FriendSNs.find(enemy.WorkObjectSN) == p3FriendSNs.end()) continue;
                if(getAttackRange(enemy.Sort) <= 1 && max(abs(edx), abs(edy)) <= 3) continue;
                int d = manhattan_distance(enemy.BlockDR, enemy.BlockUR, priestX, priestY);
                if(enemy.WorkObjectSN == priestSN) {
                    if(d < minDistAtkPriest) { minDistAtkPriest = d; atkPriestSN = enemy.SN; }
                } else if(enemy.Sort == AT_HOPLITE) {
                    if(d < minDistPhalanx) { minDistPhalanx = d; phalanxSN = enemy.SN; }
                } else {
                    if(d < minDistAny) { minDistAny = d; anySN = enemy.SN; }
                }
            }
            if(atkPriestSN != -1)    convertTargetSN = atkPriestSN;
            else if(phalanxSN != -1) convertTargetSN = phalanxSN;
            else                     convertTargetSN = anySN;
        }

        // 终局目标：视野内已建成的敌方攻城武器厂（isWin 判胜，必须转化不能摧毁）
        // 冷却期（休整/打断）内跳过——此时转化指令会被内核拒绝并强制站定
        if(p3ActionAllowed && convertTargetSN == -1 && info.GameFrame >= convertLockCooldown) {
            for(auto& b : info.enemy_buildings) {
                if(b.Type == BUILDING_SIEGE && b.Blood > 0 && b.Percent >= 100
                   && max(abs(b.BlockDR - priestX), abs(b.BlockUR - priestY)) <= VISION_PRIEST) {
                    convertTargetSN = b.SN;
                    break;
                }
            }
        }

        if(convertTargetSN != -1) {
            HumanAction(priestSN, convertTargetSN);
            converting = true;
            lastOrderFrame = info.GameFrame;
            return;
        }

        // 无转化目标（含转化休整/打断冷却期）：保命优先——
        //   1) 被任何敌人点名(WorkObjectSN==priestSN)→沿"祭司-攻击者"反方向撤退；
        //      远程点名者(祭司猎手战车弓兵等射程7)撤9格脱射程，近战撤5格
        //      （2026-10-08 修复：旧版一律撤5格，被射程7的猎手追着打，祭司来回震荡致死）
        //   2) 近战兵近身(切比雪夫<=3)→向我方中心方向撤5格
        //   3) 处于敌方箭塔射程(切比雪夫<=8)内→背塔外撤6格（塔是建筑，
        //      WorkObjectSN 点名检测捕获不到，站塔射程内会被白嫖点死）
        //   4) 无敌情→回待命点（总攻前=地图中心，总攻后=随军锚点）
        // 冷却期内无法转化，站着挨远程点射等于白给，必须动起来
        int aimX = phase3PriestTargetX, aimY = phase3PriestTargetY;
        int atkMinCheb = INT_MAX, atkX = -1, atkY = -1;
        bool atkIsRanged = false;
        bool meleeNear = false;
        for(auto& enemy : info.enemy_armies) {
            if(enemy.Blood <= 0) continue;
            int edx = enemy.BlockDR - priestX, edy = enemy.BlockUR - priestY;
            if(getAttackRange(enemy.Sort) <= 1 && max(abs(edx), abs(edy)) <= 3)
                meleeNear = true;
            if(enemy.WorkObjectSN == priestSN) {
                int d = max(abs(edx), abs(edy));
                if(d < atkMinCheb) {
                    atkMinCheb = d;
                    atkX = enemy.BlockDR; atkY = enemy.BlockUR;
                    atkIsRanged = (getAttackRange(enemy.Sort) > 1);
                }
            }
        }
        if(atkX != -1 || meleeNear) {
            double rdx, rdy;
            int retreatDist = 5;
            if(atkX != -1) {
                rdx = priestX - atkX; rdy = priestY - atkY;   // 背离最近点名者
                if(atkIsRanged) retreatDist = 9;              // 远程点名：撤出射程圈
            } else { rdx = centerX - priestX; rdy = centerY - priestY; }
            double rlen = sqrt(rdx*rdx + rdy*rdy);
            if(rlen > 1e-6) {
                aimX = max(0, min(MAP_SIZE-1, priestX + (int)(rdx/rlen*retreatDist)));
                aimY = max(0, min(MAP_SIZE-1, priestY + (int)(rdy/rlen*retreatDist)));
            } else { aimX = centerX; aimY = centerY; }
        } else {
            // 敌方箭塔射程内（未被军队点名时）：背塔外撤6格
            int twX = -1, twY = -1, twD = INT_MAX;
            for(auto& b : info.enemy_buildings) {
                if(b.Type != BUILDING_ARROWTOWER || b.Blood <= 0) continue;
                int d = max(abs(b.BlockDR - priestX), abs(b.BlockUR - priestY));
                if(d < twD) { twD = d; twX = b.BlockDR; twY = b.BlockUR; }
            }
            if(twX != -1 && twD <= 8) {
                double rdx = priestX - twX, rdy = priestY - twY;
                double rlen = sqrt(rdx*rdx + rdy*rdy);
                if(rlen > 1e-6) {
                    aimX = max(0, min(MAP_SIZE-1, priestX + (int)(rdx/rlen*6 + (rdx>=0?0.5:-0.5))));
                    aimY = max(0, min(MAP_SIZE-1, priestY + (int)(rdy/rlen*6 + (rdy>=0?0.5:-0.5))));
                }
            }
        }
        if(aimX < 0 || aimY < 0) return;   // 锚点尚未由 gamePhase3 初始化
        pair<int,int> spot = legalPlaceAround(114514, aimX, aimY);
        if(spot.first == -1) return;
        if(max(abs(spot.first - priestX), abs(spot.second - priestY)) > 1) {
            HumanMove(priestSN, spot.first * BLOCKSIDELENGTH, spot.second * BLOCKSIDELENGTH);
            lastOrderX = spot.first; lastOrderY = spot.second;
            lastOrderFrame = info.GameFrame;
        }
        return;
    }

    lastPriestBlood = priestBlood;

    // 统一节流：视野范围内有敌人时每 20 帧发一次指令，否则 50 帧，
    // 修复探索期到达目标后每帧刷 HumanMove 同一点
    // 视野按内核 getViewLab 的圆形判定（欧式距离 <= VISION_PRIEST）
    bool enemyInVision = false;
    for(auto& enemy : info.enemy_armies) {
        int dx = enemy.BlockDR - priestX;
        int dy = enemy.BlockUR - priestY;
        if(dx * dx + dy * dy <= VISION_PRIEST * VISION_PRIEST) {
            enemyInVision = true;
            break;
        }
    }
    int interval = enemyInVision ? 20 : retryInterval;
    if(info.GameFrame - lastOrderFrame < interval) {
        return;
    }

    // [priDBG] 调试：离祭祀最近的敌人 SN、其 WorkObjectSN 是否为祭祀、距离
    // （排查方阵兵冲脸时祭祀无反应问题；其余调试输出已通过 DBG_OFF 宏关闭）
    {
        int nearSN = -1, nearWorkSN = -1, nearMan = INT_MAX, nearCheb = INT_MAX;
        int nearDR = -1, nearUR = -1, nearSort = -1, nearBlood = -1;
        for(auto& enemy : info.enemy_armies) {
            int md = manhattan_distance(enemy.BlockDR, enemy.BlockUR, priestX, priestY);
            if(md < nearMan) {
                nearMan = md;
                nearCheb = max(abs(enemy.BlockDR - priestX), abs(enemy.BlockUR - priestY));
                nearSN = enemy.SN;
                nearWorkSN = enemy.WorkObjectSN;
                nearDR = enemy.BlockDR;
                nearUR = enemy.BlockUR;
                nearSort = enemy.Sort;
                nearBlood = enemy.Blood;
            }
        }
        if(nearSN != -1) {
            DebugText("[priDBG] frame=" + to_string(info.GameFrame)
                      + " priestSN=" + to_string(priestSN)
                      + " nearestEnemy SN=" + to_string(nearSN)
                      + " sort=" + to_string(nearSort)
                      + " blood=" + to_string(nearBlood)
                      + " at(" + to_string(nearDR) + "," + to_string(nearUR) + ")"
                      + " WorkObjectSN=" + to_string(nearWorkSN)
                      + " targetIsPriest=" + (nearWorkSN == priestSN ? "YES" : "NO")
                      + " manDist=" + to_string(nearMan)
                      + " chebDist=" + to_string(nearCheb)
                      + " convertTargetSN=" + to_string(convertTargetSN)
                      + " atkRangeMap=" + to_string(enemyAtkRangeMap[priestX][priestY]));
        }
    }

    // 需求：近战敌人3格以内（切比雪夫）时，围绕最近箭塔绕圈风筝，借箭塔火力消耗追击敌人
    {
        bool meleeNear = false;
        for(auto& enemy : info.enemy_armies) {
            if(getAttackRange(enemy.Sort) <= 1
               && max(abs(enemy.BlockDR - priestX), abs(enemy.BlockUR - priestY)) <= 3) {
                meleeNear = true;
                break;
            }
        }
        if(meleeNear) {
            int towerX = -1, towerY = -1, towerMinDist = INT_MAX;
            for(auto& b : info.buildings) {
                if(b.Type == BUILDING_ARROWTOWER && b.Blood >= 10) {
                    int d = manhattan_distance(b.BlockDR, b.BlockUR, priestX, priestY);
                    if(d < towerMinDist) { towerMinDist = d; towerX = b.BlockDR; towerY = b.BlockUR; }
                }
            }
            if(towerX != -1) {
                static const int ringDx[8] = {4, 4, 0, -4, -4, -4, 0, 4};
                static const int ringDy[8] = {0, 4, 4, 4, 0, -4, -4, -4};
                static int circleIdx = 0;
                static int circleTowerX = -1, circleTowerY = -1;
                if(towerX != circleTowerX || towerY != circleTowerY) {
                    circleTowerX = towerX;
                    circleTowerY = towerY;
                    int best = INT_MAX;
                    for(int k = 0; k < 8; k++) {
                        int d = max(abs(priestX - (towerX + ringDx[k])), abs(priestY - (towerY + ringDy[k])));
                        if(d < best) { best = d; circleIdx = k; }
                    }
                }
                for(int k = 0; k < 8; k++) {
                    int idx = (circleIdx + k) % 8;
                    int wx = circleTowerX + ringDx[idx];
                    int wy = circleTowerY + ringDy[idx];
                    if(wx < 0 || wx >= MAP_SIZE || wy < 0 || wy >= MAP_SIZE) continue;
                    if(gameMap[wx][wy] != 0) continue;
                    if(abs(priestX - wx) <= 1 && abs(priestY - wy) <= 1) { circleIdx = (idx + 1) % 8; continue; }
                    HumanMove(priestSN, wx * BLOCKSIDELENGTH, wy * BLOCKSIDELENGTH);
                    lastOrderX = wx;
                    lastOrderY = wy;
                    lastPriestX = priestX;
                    lastPriestY = priestY;
                    lastOrderFrame = info.GameFrame;
                    return;
                }
            }
        }
    }

    if(info.GameFrame > 5500) {
        int nextX = -1, nextY = -1;

        if(enemyAtkRangeMap[priestX][priestY] == 1) {
            const int TOWER_RANGE = 7;   // 与 arrowTowerAttack 的 attackRange 一致（切比雪夫距离）
            bool handled = false;

            // 需求：遇到攻击祭祀的远程兵种，沿箭塔攻击范围边缘撤退，直到敌人进入箭塔射程
            // 1) 找最近的可用箭塔
            int towerX = -1, towerY = -1, towerMinDist = INT_MAX;
            for(auto& b : info.buildings) {
                if(b.Type == BUILDING_ARROWTOWER && b.Blood >= 10) {
                    int d = manhattan_distance(b.BlockDR, b.BlockUR, priestX, priestY);
                    if(d < towerMinDist) { towerMinDist = d; towerX = b.BlockDR; towerY = b.BlockUR; }
                }
            }
            // 2) 找攻击祭祀的远程兵种（远程即 getAttackRange > 1）
            int enemyX = -1, enemyY = -1, enemyMinDist = INT_MAX;
            for(auto& enemy : info.enemy_armies) {
                if(enemy.WorkObjectSN == priestSN && getAttackRange(enemy.Sort) > 1) {
                    int d = manhattan_distance(enemy.BlockDR, enemy.BlockUR, priestX, priestY);
                    if(d < enemyMinDist) { enemyMinDist = d; enemyX = enemy.BlockDR; enemyY = enemy.BlockUR; }
                }
            }
            if(towerX != -1 && enemyX != -1) {
                bool enemyInTowerRange = (abs(enemyX - towerX) <= TOWER_RANGE && abs(enemyY - towerY) <= TOWER_RANGE);
                if(!enemyInTowerRange) {
                    // 需求：攻击祭祀的是战车弓兵时，取战车弓兵群中心，祭祀背离中心方向移动
                    // （群中心 = 所有攻击祭祀的战车弓兵的位置均值；远程沿塔边撤退的原逻辑只处理非战车弓兵）
                    double chariotCenterX = 0, chariotCenterY = 0;
                    int chariotAtkNum = 0;
                    for(auto& enemy : info.enemy_armies) {
                        if(enemy.WorkObjectSN == priestSN && enemy.Sort == AT_CHARIOT_ARCHER) {
                            chariotCenterX += enemy.BlockDR;
                            chariotCenterY += enemy.BlockUR;
                            chariotAtkNum++;
                        }
                    }
                    if(chariotAtkNum > 0) {
                        chariotCenterX /= chariotAtkNum;
                        chariotCenterY /= chariotAtkNum;
                        // 背离中心方向 = 祭司位置 - 群中心；步长取 6 格（战车弓兵射程外拉开距离）
                        int dirX = priestX - (int)(chariotCenterX + 0.5);
                        int dirY = priestY - (int)(chariotCenterY + 0.5);
                        if(dirX == 0 && dirY == 0) dirX = 1;   // 重合时给默认方向
                        // 归一化方向，沿方向最多走 6 格
                        double len = sqrt((double)dirX * dirX + (double)dirY * dirY);
                        // 沿方向逐格回退，直到找到界内且未被占用的合法点
                        int fleeX = -1, fleeY = -1;
                        for(int back = 6; back >= 1; back--) {
                            int tx = priestX + (int)(dirX / len * back + (dirX >= 0 ? 0.5 : -0.5));
                            int ty = priestY + (int)(dirY / len * back + (dirY >= 0 ? 0.5 : -0.5));
                            if(tx < 0 || tx >= MAP_SIZE || ty < 0 || ty >= MAP_SIZE) continue;
                            if(gameMap[tx][ty] != 0) continue;
                            fleeX = tx; fleeY = ty;
                            break;
                        }
                        if(fleeX != -1) {
                            nextX = fleeX; nextY = fleeY;
                            HumanMove(priestSN, fleeX * BLOCKSIDELENGTH, fleeY * BLOCKSIDELENGTH);
                            handled = true;
                        }
                    }
                    if(!handled) {
                    // 3) 在箭塔射程边缘(切比雪夫==TOWER_RANGE)找合法点：离远程敌人最远、离祭祀尽量近
                    int bestX = -1, bestY = -1;
                    int bestToEnemy = -1;
                    int bestToPriest = INT_MAX;
                    for(int i = 0; i < MAP_SIZE; i++) {
                        for(int j = 0; j < MAP_SIZE; j++) {
                            int cheb = max(abs(i - towerX), abs(j - towerY));
                            if(cheb != TOWER_RANGE || gameMap[i][j] != 0) continue;
                            int de = abs(i - enemyX) + abs(j - enemyY);
                            int dp = abs(i - priestX) + abs(j - priestY);
                            if(de > bestToEnemy || (de == bestToEnemy && dp < bestToPriest)) {
                                bestToEnemy = de;
                                bestToPriest = dp;
                                bestX = i; bestY = j;
                            }
                        }
                    }
                    if(bestX != -1) {
                        nextX = bestX; nextY = bestY;
                        HumanMove(priestSN, bestX * BLOCKSIDELENGTH, bestY * BLOCKSIDELENGTH);
                        handled = true;
                    }
                    }
                }
            }

            if(!handled) {
                int minDis = 10000;
                for(int i = 0; i < MAP_SIZE; i++) {
                    for(int j = 0; j < MAP_SIZE; j++) {
                        if(enemyAtkRangeMap[i][j] == 0 && gameMap[i][j] == 0 && manhattan_distance(i,j,priestX,priestY) < minDis) {
                            minDis = manhattan_distance(i,j,priestX,priestY);
                            nextX = i;
                            nextY = j;
                        }
                    }
                }
                HumanMove(priestSN, nextX * BLOCKSIDELENGTH, nextY * BLOCKSIDELENGTH);
            }
        } else {
            // 需求7：不在任何仇恨敌人攻击范围内时，转化仇恨在我方箭塔身上的敌军单位
            // 约束1：只能选择 WorkObjectSN 指向我方箭塔的敌人（正被箭塔攻击/吸引火力的单位，转化时机最安全）
            // 约束2：该次转化成功前只锁定该单位——目标从 enemy_armies 消失（转化成功/阵亡）才允许换目标

            // 目标锁定：当前目标仍存活则继续下达同一目标（含被打断后重新读条的情形）
            bool targetAlive = false;
            bool targetInvalid = false;   // 阔剑兵(不得转化) → 解锁
            for(auto& enemy : info.enemy_armies) {
                if(enemy.SN == convertTargetSN && enemy.Blood > 0) {
                    targetAlive = true;
                    targetInvalid = (enemy.Sort == AT_BROADSWORDSMAN);
                    break;
                }
            }
            if(convertTargetSN != -1 && (!targetAlive || targetInvalid)) {
                convertTargetSN = -1;   // 转化成功/目标阵亡/目标为阔剑兵，解锁换目标
            }

            if(convertTargetSN == -1 && info.GameFrame >= convertLockCooldown) {
                // 收集我方友军（箭塔 + 我方单位/军队）SN 集合
                set<int> myFriendSNs;
                for(auto& b : info.buildings) {
                    if(b.Blood > 0) myFriendSNs.insert(b.SN);   // 所有友方建筑（含箭塔）
                }
                for(auto& army : info.armies) {
                    if(army.Blood > 0) myFriendSNs.insert(army.SN);   // 我方军队
                }
                // 三级筛选：仇恨已在友军（含箭塔/祭祀）身上的前提下——
                //   优先级1：正在攻击祭祀的敌人（WorkObjectSN==priestSN，解除自身威胁最直接）
                //   优先级2：方阵兵(AT_HOPLITE)——第二波敌军主力，优先转化削弱/策反
                //   优先级3：其它仇恨在友军身上的敌军单位（含投石车——2026-10-08 起
                //            投石车不再保留给祭祀转化，部队/箭塔可直接射杀）
                // 阔剑兵(AT_BROADSWORDSMAN)不得转化（2026-10-08，第二波规则）
                // 同级取距离最近者；近战敌人贴脸(切比雪夫<=3)时跳过（读条必被打断）
                int minDistAtkPriest = INT_MAX, minDistPhalanx = INT_MAX, minDistAny = INT_MAX;
                int atkPriestSN = -1, phalanxSN = -1, anySN = -1;
                for(auto& enemy : info.enemy_armies) {
                    if(enemy.Blood <= 0) continue;
                    if(enemy.Sort == AT_BROADSWORDSMAN) continue;   // 阔剑兵不得转化
                    // 只转化视野范围内的敌人，避免祭司跑出大本营追远处的敌军
                    int edx = enemy.BlockDR - priestX;
                    int edy = enemy.BlockUR - priestY;
                    if(edx * edx + edy * edy > VISION_PRIEST * VISION_PRIEST) continue;
                    if(myFriendSNs.find(enemy.WorkObjectSN) == myFriendSNs.end()) continue;   // 仇恨必须在友军（含箭塔）身上
                    if(getAttackRange(enemy.Sort) <= 1 && max(abs(edx), abs(edy)) <= 3) continue;
                    int d = manhattan_distance(enemy.BlockDR, enemy.BlockUR, priestX, priestY);
                    if(enemy.WorkObjectSN == priestSN) {
                        if(d < minDistAtkPriest) { minDistAtkPriest = d; atkPriestSN = enemy.SN; }
                    } else if(enemy.Sort == AT_HOPLITE) {
                        if(d < minDistPhalanx) { minDistPhalanx = d; phalanxSN = enemy.SN; }
                    } else {
                        if(d < minDistAny) { minDistAny = d; anySN = enemy.SN; }
                    }
                }
                if(atkPriestSN != -1)      convertTargetSN = atkPriestSN;
                else if(phalanxSN != -1)   convertTargetSN = phalanxSN;
                else                       convertTargetSN = anySN;
            }

            if(convertTargetSN != -1) {
                // 转化目标保护（2026-10-08）：登记为保留目标，箭塔/防守部队不得射杀；
                // 每次锁定/续锁都刷新时间戳，祭祀休整期(20秒)内保护不过期
                priestReserveSN = convertTargetSN;
                priestReserveFrame = info.GameFrame;
                HumanAction(priestSN, convertTargetSN);  // 转化（内核 ATTACKTYPE_CHANGE）
                converting = true;
                nextX = priestX;  // 未移动，占位避免 lastOrder 记录越界
                nextY = priestY;
            } else {
                // 防御前压：敌军正在攻击我方建筑（已在我方基地范围内）且祭司自身
                // 未被点名、无近战威胁时，向该敌人前压至视野圆内缘——
                // 攻击建筑的敌人仇恨被建筑/箭塔吸住，站定转化最安全。
                // 否则祭司全程守家，眼睁睁看着投石车等远程敌军在视野外(12格外)拆家。
                bool advanced = false;
                {
                    bool priestHunted = false, meleeNearby = false;
                    for(auto& enemy : info.enemy_armies) {
                        if(enemy.Blood <= 0) continue;
                        if(enemy.WorkObjectSN == priestSN) { priestHunted = true; break; }
                        if(getAttackRange(enemy.Sort) <= 1
                           && max(abs(enemy.BlockDR - priestX), abs(enemy.BlockUR - priestY)) <= 4) {
                            meleeNearby = true; break;
                        }
                    }
                    if(!priestHunted && !meleeNearby && info.GameFrame >= convertLockCooldown) {
                        set<int> myBuildingSNs;
                        for(auto& b : info.buildings) if(b.Blood > 0) myBuildingSNs.insert(b.SN);
                        int advDist = INT_MAX, advX = 0, advY = 0;
                        for(auto& enemy : info.enemy_armies) {
                            if(enemy.Blood <= 0) continue;
                            if(myBuildingSNs.find(enemy.WorkObjectSN) == myBuildingSNs.end()) continue;
                            int d = manhattan_distance(enemy.BlockDR, enemy.BlockUR, priestX, priestY);
                            if(d < advDist) { advDist = d; advX = enemy.BlockDR; advY = enemy.BlockUR; }
                        }
                        int ex = advX - priestX, ey = advY - priestY;
                        double len = sqrt((double)ex * ex + (double)ey * ey);
                        if(advDist != INT_MAX && advDist <= 35 && len > VISION_PRIEST) {
                            // 目标在视野外：推进到距其 VISION_PRIEST-3 格处（视野圆内缘）
                            int stopX = advX - (int)(ex / len * (VISION_PRIEST - 3) + (ex >= 0 ? 0.5 : -0.5));
                            int stopY = advY - (int)(ey / len * (VISION_PRIEST - 3) + (ey >= 0 ? 0.5 : -0.5));
                            stopX = max(0, min(MAP_SIZE - 1, stopX));
                            stopY = max(0, min(MAP_SIZE - 1, stopY));
                            pair<int,int> spot = legalPlaceAround(114514, stopX, stopY);
                            if(spot.first != -1 && max(abs(spot.first - priestX), abs(spot.second - priestY)) > 1) {
                                HumanMove(priestSN, spot.first * BLOCKSIDELENGTH, spot.second * BLOCKSIDELENGTH);
                                nextX = spot.first;
                                nextY = spot.second;
                                advanced = true;
                                DebugText("[priDBG] 防御前压：敌军正攻击我方建筑，推进至 ("
                                          + to_string(spot.first) + "," + to_string(spot.second)
                                          + ") frame=" + to_string(info.GameFrame));
                            }
                        }
                    }
                }
                if(!advanced) {
                // 无转化目标也无前压需求：回到箭塔连线中点后方的驻留点待命。
                // 2026-10-08 优化：原"后移10格"驻留点靠后，贴塔方阵兵常处12格
                // 转化视野边缘，转化效率低；现默认后移5格——贴塔近战敌兵距祭祀
                // 约9~10格，稳处视野内可立即锁定转化。
                // 安全策略（保证祭祀不被任何敌军设为攻击目标）：
                //   1) 敌方野外未锁定单位会对6格内玩家单位主动索敌(enemyai
                //      FIELD_LAND_AGGRO_RADIUS=6)，故驻留点须距所有"未锁定我方
                //      目标"的自由敌军 >=7格(直线)；
                //   2) 已锁定我方建筑/单位的敌军(WorkObjectSN指向我方)不会改打
                //      祭祀，可安全接近（波次进攻单位均属此类）；
                //   3) 默认5格落点不满足1)时，按 5/7/9/12 梯度逐级后移直至安全。
                // 防震荡：legalPlaceAround 受村民瞬时占位影响会抖动，新落点与
                // 上次下令点相差<=2格时不重复下令（修复驻留点来回跳动）。
                int midX = 0, midY = 0;
                int arrowTowerNum = 0;
                for(auto& building: info.buildings) {
                    if(building.Type == BUILDING_ARROWTOWER && building.Blood >= 10) {
                        midX += building.BlockDR;
                        midY += building.BlockUR;
                        arrowTowerNum++;
                    }
                }
                if(arrowTowerNum == 0) {
                    DBG_OFF("No arrow tower");
                    return;
                }
                midX /= arrowTowerNum;
                midY /= arrowTowerNum;
                double dirX = midX - 50, dirY = midY - 50;
                double len = sqrt(dirX * dirX + dirY * dirY);
                if(len < 0.001) { dirX = 1; dirY = 0; len = 1; }   // 中点恰在地图中心时取默认方向
                // 我方建筑+军队 SN 集合：WorkObjectSN 指向其中的敌军视为"已锁定"，不会改打祭祀
                set<int> ourSNs;
                for(auto& b : info.buildings) if(b.Blood > 0) ourSNs.insert(b.SN);
                for(auto& a : info.armies)   if(a.Blood > 0) ourSNs.insert(a.SN);

                pair<int,int> target(-1, -1);
                static const int backShifts[] = {5, 7, 9, 12};
                for(int si = 0; si < 4; si++) {
                    int cx = midX + (int)(dirX / len * backShifts[si] + (dirX >= 0 ? 0.5 : -0.5));
                    int cy = midY + (int)(dirY / len * backShifts[si] + (dirY >= 0 ? 0.5 : -0.5));
                    cx = max(0, min(MAP_SIZE - 1, cx));
                    cy = max(0, min(MAP_SIZE - 1, cy));
                    pair<int,int> spot = legalPlaceAround(114514, cx, cy);
                    if(spot.first == -1) continue;
                    bool safe = true;
                    for(auto& enemy : info.enemy_armies) {
                        if(enemy.Blood <= 0) continue;
                        if(ourSNs.find(enemy.WorkObjectSN) != ourSNs.end()) continue;   // 已锁定敌军：安全
                        int dx = enemy.BlockDR - spot.first, dy = enemy.BlockUR - spot.second;
                        if(dx * dx + dy * dy < 49) { safe = false; break; }   // 距自由敌军<7格：不安全
                    }
                    if(safe) { target = spot; break; }
                }
                if(target.first == -1) {
                    // 全梯度落点均不安全或非法：取最远档(12格)兜底，余下交给逃逸逻辑接管
                    int cx = midX + (int)(dirX / len * 12 + (dirX >= 0 ? 0.5 : -0.5));
                    int cy = midY + (int)(dirY / len * 12 + (dirY >= 0 ? 0.5 : -0.5));
                    cx = max(0, min(MAP_SIZE - 1, cx));
                    cy = max(0, min(MAP_SIZE - 1, cy));
                    target = legalPlaceAround(114514, cx, cy);
                    if(target.first == -1) {
                        DBG_OFF("No legal place for priest");
                        return;
                    }
                }
                nextX = target.first;
                nextY = target.second;
                // 防震荡：与上次下令点相差<=2格不重复下令（村民占位抖动不影响驻留）
                if(max(abs(target.first - lastOrderX), abs(target.second - lastOrderY)) > 2) {
                    HumanMove(priestSN, target.first * BLOCKSIDELENGTH, target.second * BLOCKSIDELENGTH);
                }
                }
            }
        }

        lastOrderX = nextX;
        lastOrderY = nextY;
        lastPriestX = priestX;
        lastPriestY = priestY;
        lastOrderFrame = info.GameFrame;
        return;
    }
    
    if(lastPriestX != -1 && lastOrderX != -1 && manhattan_distance(lastOrderX,lastOrderY,priestX,priestY) > 2) {
        if(info.GameFrame - lastOrderFrame > retryInterval && manhattan_distance(lastPriestX,lastPriestY,priestX,priestY) <= 1) {
            visitedPoints.push_back({lastOrderX, lastOrderY});
        } else {
            return;
        }       
    }

    for(auto& point : visitedPoints) {
        for (int dx = -radius; dx <= radius; ++dx) {
            for (int dy = -radius; dy <= radius; ++dy) {
                int nx = point.first + dx;
                int ny = point.second + dy;
                if (nx >= 0 && nx < MAP_SIZE && ny >= 0 && ny < MAP_SIZE) {
                    exploreValueMap[nx][ny] = 0;
                }
            }
        }
        exploreValueMap[point.first][point.second] -= 2;
    }

    priority_queue<tuple<int,int,int> > candPoints;

    //DBG_OFF("priestX = " + to_string(priestX) + ", priestY = " + to_string(priestY));
    //DBG_OFF("centerX = " + to_string(centerX) + ", centerY = " + to_string(centerY));
    for(int i=0;i<MAP_SIZE;i++) {
        for(int j=0;j<MAP_SIZE;j++) {
            if(manhattan_distance(i,j,centerX,centerY) > 45 || gameMap[i][j] == -1 || gameMap[i][j] == 1) continue;
            if(i == priestX && j == priestY) continue;
            int totalValue = 0;
            for (int dx = -radius; dx <= radius; ++dx) {
                for (int dy = -(radius - abs(dx)); dy <= radius - abs(dx); ++dy) {
                    int nx = i + dx;
                    int ny = j + dy;
                    if (nx >= 0 && nx < MAP_SIZE && ny >= 0 && ny < MAP_SIZE) {
                        totalValue += exploreValueMap[nx][ny];
                    }
                }
            }     
            totalValue -= manhattan_distance(i,j,priestX,priestY) ;
            //DBG_OFF("i = " + to_string(i) + ", j = " + to_string(j) + "expV = " + to_string(totalValue));
            candPoints.push(make_tuple(totalValue, i, j));
        }
    }

    if(candPoints.empty()) return;
    int bestX = get<1>(candPoints.top());
    int bestY = get<2>(candPoints.top());

    //DBG_OFF("bestX = " + to_string(bestX) + ", bestY = " + to_string(bestY) + ", expV = " + to_string(get<0>(candPoints.top())) + ", map = " + to_string(gameMap[bestX][bestY]));
    HumanMove(priestSN, (bestX + 0.5) * BLOCKSIDELENGTH, (bestY + 0.5) * BLOCKSIDELENGTH);
    lastOrderX = bestX;
    lastOrderY = bestY;
    lastPriestX = priestX;
    lastPriestY = priestY;
    lastOrderFrame = info.GameFrame;
    visitedPoints.push_back({bestX, bestY});
    
}

pair<int,int> UsrAI::legalPlaceAround(int buildingType, int cx, int cy)
{
    //DBG_OFF("cx = " + to_string(cx) + ", cy = " + to_string(cy));
    //DBG_OFF("buildingType = " + to_string(buildingType));
    int size = getBuildingSideLen(buildingType);
    for (int radius = 0; radius <= 8 ; ++radius) {
        for (int dx = -radius; dx <= radius; ++dx) {
            for (int dy = -radius; dy <= radius; ++dy) {
                int nx = cx + dx;
                int ny = cy + dy;
                if (nx >= 0 && nx + size <= MAP_SIZE && ny >= 0 && ny + size <= MAP_SIZE) {
                    int isLegal = true;
                    // 农场不需要与周边建筑留 1 格空隙：只检查自身占地(0..size-1)；
                    // 其余建筑仍外扩 1 格(-1..size)检查
                    int lo = (buildingType == BUILDING_FARM) ? 0 : -1;
                    int hi = (buildingType == BUILDING_FARM) ? size - 1 : size;
                    for(int i=lo;i<=hi && isLegal;i++) {
                        for(int j=lo;j<=hi;j++) {
                            if(gameMap[nx+i][ny+j] != 0) {
                                isLegal = false;
                                break;
                            }
                        }
                    }
                    if(isLegal) return {nx, ny};
                }
            }     
        }
    }
    return {-1, -1};
}

void UsrAI::assignBuilding()
{
    static int lastOrderId = -1;
    static pair<int,int> lastTarget;
    static int lastTryFrame = -10000;   // HumanBuild 重试节流，防失败时每帧重复下单
    const int BUILD_RETRY_INTERVAL = 30;

    if(lastOrderId != -1) {
        auto it = info.ins_ret.find(lastOrderId);
        if(it != info.ins_ret.end()) {
            // 收到回执才复位；未收到期间靠下方"走路/施工中不下单"门禁防止重复 HumanBuild
            if(it->second == ACTION_SUCCESS) {
                auto task = buildTasks.front();
                for(auto& building: info.buildings) {
                    if(building.BlockDR == lastTarget.first && building.BlockUR == lastTarget.second) {
                        for(int& sn : task.builderSNs) {
                            HumanAction(sn, building.SN);
                        }
                        break;
                    }
                }
                buildTasks.erase(buildTasks.begin());
            }
            lastOrderId = -1;
        }
    }

    tagFarmer builder;
    bool builderFound = false;
    for(auto& farmer: info.farmers) {
        if(farmer.SN == builderSN) {
            builder = farmer;
            builderFound = true;
            break;
        }
    }
    if(!builderFound) return;   // 建筑工不存在（刚阵亡，init() 下帧会重选），不用无效SN下发指令
    if(buildTasks.empty()) return;

    auto task = buildTasks.front();
    if(task.BlockDR > MAP_SIZE) return;

    // 可下单时机（2026-10-07 修复）：
    //  旧逻辑只认 NowState==IDLE，但建筑工备料砍树时一棵树砍完仅 IDLE 一帧，
    //  同一帧 logging() 已经抢先派了新树（状态变 WALKING），HumanBuild 永远发不出去，
    //  表现为"木材充足却一直砍树不建房"。
    //  现允许从砍树中直接抢占：内核 addRelation 会自动挂起伐木关系。
    //  绝不打断的情况：
    //   - WALKING：可能正在去本任务工地的路上（防重复下单），也可能在去修塔/搬运
    //   - WORKING 且 WorkObjectSN 指向我方建筑：正在打地基建房或修箭塔
    bool workingOnBuilding = false;
    bool choppingTree = false;
    if(builder.NowState == HUMAN_STATE_WORKING && builder.WorkObjectSN > 0) {
        for(auto& b : info.buildings) {
            if(b.SN == builder.WorkObjectSN) { workingOnBuilding = true; break; }
        }
        for(auto& r : info.resources) {
            if(r.SN == builder.WorkObjectSN && r.Type == RESOURCE_TREE) { choppingTree = true; break; }
        }
    }
    bool canIssue = (builder.NowState == HUMAN_STATE_IDLE) || choppingTree;
    if(!canIssue || workingOnBuilding) return;
    if(info.GameFrame - lastTryFrame < BUILD_RETRY_INTERVAL) return;

    if(checkResource(task.Type)) {
        pair<int,int> target = legalPlaceAround(task.Type, task.BlockDR, task.BlockUR);
        // 房屋兜底：首批房屋周围 8 格环被基地建筑/资源占满时，改到市中心周围找点
        if(target.first == -1 && task.Type == BUILDING_HOME
           && (task.BlockDR != centerX || task.BlockUR != centerY)) {
            target = legalPlaceAround(task.Type, centerX, centerY);
        }
        if(target.first == -1) {
            DBG_OFF("[buildDBG] No legal place for Type=" + to_string(task.Type)
                    + " anchor=(" + to_string(task.BlockDR) + "," + to_string(task.BlockUR)
                    + ") frame=" + to_string(info.GameFrame));
            return;
        }
        lastTryFrame = info.GameFrame;
        lastOrderId = HumanBuild(builder.SN, task.Type, target.first, target.second);
        lastTarget = target;
        DebugText("[buildDBG] HumanBuild Type=" + to_string(task.Type)
                  + " @(" + to_string(target.first) + "," + to_string(target.second)
                  + ") wood=" + to_string(info.Wood) + " frame=" + to_string(info.GameFrame));
    }
}

// 未完工建筑看护：任何我方地基(Percent<100)若无人修建，则从伐木组/无组空闲村民中
// 派最近者加入建造。背景：任务在人放下地基后即从 buildTasks 移除，地基完工完全依赖
// 建筑工的自动FixBuilding关系——一旦建筑工被其它HumanBuild/抽调打断（内核addRelation
// 自动挂起旧关系），地基就会以99%的形态永久搁置，只能手动补救。
void UsrAI::fixUnstaffedFoundations()
{
    static int lastPollFrame = -1000;
    if(info.GameFrame - lastPollFrame < 45) return;   // ~1.8s 轮询一次
    lastPollFrame = info.GameFrame;

    // 1) 收集未完工建筑（农田由 farmingIdlePoll 专门负责，跳过）
    vector<tagBuilding> needFix;
    for(auto& b : info.buildings) {
        if(b.Blood <= 0 || b.Percent >= 100 || b.Type == BUILDING_FARM) continue;
        needFix.push_back(b);
    }
    if(needFix.empty()) return;

    // 2) 过滤已有人在修（含正在赶路）的：修建者 WorkObjectSN 已指向该建筑SN
    vector<tagBuilding> unstaffed;
    for(auto& b : needFix) {
        bool staffed = false;
        for(auto& f : info.farmers)
            if(f.Blood > 0 && f.WorkObjectSN == b.SN) { staffed = true; break; }
        if(!staffed)
            for(auto& a : info.armies)
                if(a.Blood > 0 && a.WorkObjectSN == b.SN) { staffed = true; break; }
        if(!staffed) unstaffed.push_back(b);
    }
    if(unstaffed.empty()) return;

    // 3) 候选村民：伐木组（可被HumanAction安全接管，完工后自动回伐木）+ 无组空闲村民
    //    建造任务队列非空时不抽调建筑工（他要执行队首任务）；
    //    维修组由 fixingArrowTower 专职调度；已在修其它地基的不重复抽调
    set<int> busyOnBuilding;
    for(auto& b : needFix) busyOnBuilding.insert(b.SN);
    set<int> usedSN;
    int dispatched = 0;
    for(auto& b : unstaffed) {
        if(dispatched >= 2) break;   // 每拍最多派2人，避免一次性抽空伐木组
        int bestSN = -1, bestDist = INT_MAX;
        for(auto& f : info.farmers) {
            if(f.Blood <= 0 || usedSN.count(f.SN)) continue;
            if(f.SN == builderSN && !buildTasks.empty()) continue;
            if(repairFarmersSN.count(f.SN)) continue;
            if(busyOnBuilding.count(f.WorkObjectSN)) continue;
            if(!treeFarmersSN.count(f.SN) && f.NowState != 0) continue;   // 非伐木组仅收编空闲者
            int d = manhattan_distance(f.BlockDR, f.BlockUR, b.BlockDR, b.BlockUR);
            if(d < bestDist) { bestDist = d; bestSN = f.SN; }
        }
        if(bestSN != -1) {
            HumanAction(bestSN, b.SN);
            usedSN.insert(bestSN);
            dispatched++;
            DebugText("[fixWatch] 村民 " + to_string(bestSN) + " 加入未完工建筑 Type="
                      + to_string(b.Type) + " @(" + to_string(b.BlockDR) + "," + to_string(b.BlockUR)
                      + ") Percent=" + to_string(b.Percent) + " frame=" + to_string(info.GameFrame));
        }
    }
}

double UsrAI::euclidean_distance(double x1, double y1, double x2, double y2)
{
    return sqrt((x1 - x2) * (x1 - x2) + (y1 - y2) * (y1 - y2));
}

int UsrAI::manhattan_distance(int x1, int y1, int x2, int y2)
{
    return abs(x1 - x2) + abs(y1 - y2);
}

// K-means 找“数量多且聚集紧密”的金矿簇（确定性，每帧结果一致）
GoldCluster UsrAI::findBestGoldCluster()
{
    GoldCluster best;
    best.centerX = -1; best.centerY = -1;

    vector< pair<double,double> > pts;
    vector<int> sns;
    for (auto& res : info.resources) {
        if (res.Type == RESOURCE_GOLD) {
            pts.push_back(make_pair((double)res.BlockDR, (double)res.BlockUR));
            sns.push_back(res.SN);
        }
    }
    if (pts.empty()) return best;
    if (pts.size() == 1) {
        best.centerX = (int)pts[0].first;
        best.centerY = (int)pts[0].second;
        best.sns.push_back(sns[0]);
        return best;
    }

    double bestScore = -1;
    int Kmax = (int)min((size_t)3, pts.size());
    for (int K = 1; K <= Kmax; K++) {
        // 确定性 k-means++ 初始化：每次选距已有质心最远的点
        vector< pair<double,double> > centers;
        centers.push_back(pts[0]);
        while ((int)centers.size() < K) {
            int farIdx = 0; double farD = -1;
            for (int i = 0; i < (int)pts.size(); i++) {
                double d = 1e18;
                for (int j = 0; j < (int)centers.size(); j++) {
                    double ddx = pts[i].first - centers[j].first;
                    double ddy = pts[i].second - centers[j].second;
                    d = min(d, ddx*ddx + ddy*ddy);
                }
                if (d > farD) { farD = d; farIdx = i; }
            }
            centers.push_back(pts[farIdx]);
        }

        // 迭代分配+更新质心
        vector<int> assign(pts.size(), 0);
        for (int iter = 0; iter < 20; iter++) {
            bool changed = false;
            for (int i = 0; i < (int)pts.size(); i++) {
                int bj = 0; double bd = 1e18;
                for (int j = 0; j < K; j++) {
                    double ddx = pts[i].first - centers[j].first;
                    double ddy = pts[i].second - centers[j].second;
                    double d = ddx*ddx + ddy*ddy;
                    if (d < bd) { bd = d; bj = j; }
                }
                if (assign[i] != bj) { assign[i] = bj; changed = true; }
            }
            if (!changed) break;
            for (int j = 0; j < K; j++) {
                double sx = 0, sy = 0; int c = 0;
                for (int i = 0; i < (int)pts.size(); i++)
                    if (assign[i] == j) { sx += pts[i].first; sy += pts[i].second; c++; }
                if (c > 0) centers[j] = make_pair(sx / c, sy / c);
            }
        }

        // 评分各簇（count-1)/(1+spread)，跨 K 取最优
        for (int j = 0; j < K; j++) {
            int c = 0; double sx = 0, sy = 0;
            for (int i = 0; i < (int)pts.size(); i++)
                if (assign[i] == j) { c++; sx += pts[i].first; sy += pts[i].second; }
            if (c == 0) continue;
            double cx = sx / c, cy = sy / c;
            double spread = 0;
            for (int i = 0; i < (int)pts.size(); i++)
                if (assign[i] == j)
                    spread += sqrt((pts[i].first-cx)*(pts[i].first-cx)
                                 + (pts[i].second-cy)*(pts[i].second-cy));
            spread /= c;
            double score = (c - 1) / (1.0 + spread);
            if (score > bestScore) {
                bestScore = score;
                best.centerX = (int)cx;
                best.centerY = (int)cy;
                best.sns.clear();
                for (int i = 0; i < (int)pts.size(); i++)
                    if (assign[i] == j) best.sns.push_back(sns[i]);
            }
        }
    }
    return best;
}

int UsrAI::getNearestResource(int resourceType, int cx, int cy, int deadoraliveOnly)
{
    priority_queue<pair<double,int> > pq;
    for(auto& resource : info.resources) {
        if(resource.Type == resourceType) {
            if(deadoraliveOnly != -1) {
                if(resource.Type == RESOURCE_GAZELLE && deadoraliveOnly == 1 && resource.Blood > 0) continue;
                if(resource.Type == RESOURCE_GAZELLE && deadoraliveOnly == 0 && resource.Blood <= 0) continue;
            }
            pq.push(make_pair(euclidean_distance(resource.BlockDR, resource.BlockUR, (double)cx * BLOCKSIDELENGTH, (double)cy * BLOCKSIDELENGTH), resource.SN));
        }
    }
    if(pq.empty()) {
        nearestResourceX = - 100 * BLOCKSIDELENGTH;
        nearestResourceY = - 100 * BLOCKSIDELENGTH;
        return -1;
    }
    for(auto& resource : info.resources) {
        if(resource.SN == pq.top().second) {
            nearestResourceX = resource.DR;
            nearestResourceY = resource.UR;
            return pq.top().second;
        }
    }
    return -1;  // 兜底（理论不可达）
}

void UsrAI::collecting()
{
    static int lastUpdateFrame = -1;
    static int buildingScheduled = 0;
    if(deadMeatResources.empty()) {
        if(info.GameFrame - lastUpdateFrame > 25) {
            // 收集当前所有可用羚羊尸体（Blood<=0），并记录肉量最多的一具
            vector<int> corpseSNs;
            map<int,int> corpseCnt;
            int maxCnt = -1, maxCntSN = -1;
            for(auto& resource : info.resources) {
                if(resource.Type == RESOURCE_GAZELLE && resource.Blood <= 0) {
                    corpseSNs.push_back(resource.SN);
                    corpseCnt[resource.SN] = resource.Cnt;
                    if(resource.Cnt > maxCnt) {
                        maxCnt = resource.Cnt;
                        maxCntSN = resource.SN;
                    }
                }
            }
            if(maxCntSN == -1) {
                // 地图上无任何羚羊尸体：回狩猎相位（2026-10-08 修复）
                // 旧版直接 return，huntingPhase 卡 1 且无尸可收，猎人永久原地待命
                huntingPhase = 0;
                lastUpdateFrame = -1;
                farmerState.clear();
                return;
            }
            // 分配：
            //   原猎人            → 集中采集肉量最多的尸体（原有行为）
            //   收尸帮工(伐木转入) → 均分到所有尸体上（每具 corpseSNs 尽量分到相同人数，
            //                        人数并列时优先派往剩余肉量多的尸体）
            map<int,int> corpseLoad;   // 尸体SN -> 已分配帮工数
            for(auto& farmer : info.farmers) {
                if(hunterFarmersSN.count(farmer.SN) == 0) continue;  // 只派猎人收肉
                int targetSN = maxCntSN;
                if(corpseHelperSNs.count(farmer.SN) && !corpseSNs.empty()) {
                    int bestLoad = INT_MAX, bestCnt = -1;
                    for(int sn : corpseSNs) {
                        int load = corpseLoad.count(sn) ? corpseLoad[sn] : 0;
                        if(load < bestLoad || (load == bestLoad && corpseCnt[sn] > bestCnt)) {
                            bestLoad = load;
                            bestCnt = corpseCnt[sn];
                            targetSN = sn;
                        }
                    }
                }
                corpseLoad[targetSN]++;

                if(farmer.NowState == 0 && farmerState.count(farmer.SN) != 0 && farmerState[farmer.SN] == 0
                    || farmer.NowState == 1 && farmerState.count(farmer.SN) != 0 && farmerState[farmer.SN] == 1) {
                    HumanAction(farmer.SN, targetSN);
                }
                if(currentFarmersSN.find(farmer.SN) == currentFarmersSN.end()) {
                    currentFarmersSN.insert(farmer.SN);
                    HumanAction(farmer.SN, targetSN);
                }
                farmerState[farmer.SN] = farmer.NowState;
            }
            lastUpdateFrame = info.GameFrame;
        }
        bool allCollected = true;
        for(auto& resource: info.resources) {
            // 只把"还有肉可采"的尸体(Blood<=0 且 Cnt>0)视为未收集完。
            // 内核中采完肉的尸体实体(Blood<=0, Cnt==0)会长期残留，
            // 若只按 Blood<=0 判定，allCollected 永远为 false，
            // huntingPhase 永远卡在 1，猎人永远无法释放转伐木
            if(resource.Type == RESOURCE_GAZELLE && resource.Blood <= 0 && resource.Cnt > 0) {
                allCollected = false;
                break;
            }
        }
        if(allCollected) {
            lastUpdateFrame = -1;
            farmerState.clear();
            huntingPhase = 0;
        }
    } else {
        if(!buildingScheduled) {
            vector<int> sns;
            for(auto& farmer: info.farmers) {
                // 只派猎人参与建仓库，避免覆盖浆果/采石/维修村民的 HumanAction 指令
                if(farmer.SN != builderSN && hunterFarmersSN.count(farmer.SN)) {
                    sns.push_back(farmer.SN);
                }
            }
            stockX /= deadMeatResources.size();
            stockY /= deadMeatResources.size();
            DBG_OFF("stockX = " + to_string(stockX));
            DBG_OFF("stockY = " + to_string(stockY));
            buildTasks.push_back({BUILDING_STOCK, stockX, stockY, {sns}});

            // 计算新箭塔坐标：参考已有箭塔(arrowTowerX,arrowTowerY)
            // 连线斜率由参考点所在象限决定，目标是让线段尽可能"穿过"对应对角线
            //   第一、三象限 -> 斜率1，垂直于对角线D1: x+y=100
            //   其余象限    -> 斜率-1，垂直于对角线D2: x-y=0
            // 约束：线段上所有点需被两塔射程覆盖(切比雪夫<=7)，故两塔切比雪夫距离<=14
            // 目标：最小化线段上点到对角线的垂直距离的最小值
            //   能穿过时最小值为0；无法穿过时让新塔尽量靠近对角线
            int newArrowTowerX, newArrowTowerY;
            {
                int refX = (arrowTowerX >= 0) ? arrowTowerX : 50;
                int refY = (arrowTowerY >= 0) ? arrowTowerY : 50;
                const int R = 7;                 // 箭塔射程(块)
                const int MAX_T = 8;          // 两箭塔间隔(切比雪夫偏移)=8（2026-10-08需求）
                const int BOUND = MAP_SIZE - 1;  // 地图上界

                if ((refX < 50 && refY > 50) || (refX > 50 && refY < 50)) {
                    // 第一、三象限：连线斜率1，B=(refX+t, refY+t)，目标穿过 D1(x+y=100)
                    int s = refX + refY - 100;  // 参考点到D1的有符号距离(未归一化)
                    int t;
                    if (s > 0) {
                        // 向x+y减小方向，取可行的最小t使B尽量靠近/越过D1
                        t = max(-MAX_T, -refX);
                        t = max(t, -refY);
                    } else if (s < 0) {
                        // 向x+y增大方向
                        t = min(MAX_T, BOUND - refX);
                        t = min(t, BOUND - refY);
                    } else {
                        // 参考点已在D1上，任取正向即可穿过
                        t = min(MAX_T, BOUND - refX);
                        t = min(t, BOUND - refY);
                    }
                    if (t == 0) t = (s >= 0) ? -1 : 1;  // 避免与已有塔重合
                    newArrowTowerX = refX + t;
                    newArrowTowerY = refY + t;
                } else {
                    // 其余象限：连线斜率-1，B=(refX+t, refY-t)，目标穿过 D2(x-y=0)
                    int s = refX - refY;  // 参考点到D2的有符号距离(未归一化)
                    int t;
                    if (s > 0) {
                        // 向x-y减小方向
                        t = max(-MAX_T, -refX);
                        t = max(t, refY - BOUND);
                    } else if (s < 0) {
                        // 向x-y增大方向
                        t = min(MAX_T, BOUND - refX);
                        t = min(t, refY);
                    } else {
                        // 参考点已在D2上
                        t = min(MAX_T, BOUND - refX);
                        t = min(t, refY);
                    }
                    if (t == 0) t = (s >= 0) ? -1 : 1;
                    newArrowTowerX = refX + t;
                    newArrowTowerY = refY - t;
                }
            }
            buildTasks.push_back({BUILDING_ARROWTOWER, newArrowTowerX, newArrowTowerY, {}});
            buildTasks.push_back({BUILDING_HOME, homeX, homeY, {}});
            buildingScheduled = 1;
        }
        if(!stockBuilt) return;

        nearestGazelleSN = -1;
        int i = 0;
        for(auto& farmer: info.farmers) {
            if(hunterFarmersSN.count(farmer.SN) == 0) continue;  // 只派猎人收肉
            farmerTask[farmer.SN] = deadMeatResources[i];
            currentFarmersSN.insert(farmer.SN);
            HumanAction(farmer.SN, deadMeatResources[i]); 
            i++;
            if(i >= deadMeatResources.size()) i = 0;
        }
        deadMeatResources.clear();
    }
}

void UsrAI::hunting()
{
    // [huntDBG] 每 60 帧打印一次狩猎状态快照
    static int lastHuntDbgFrame = -10000;
    if(info.GameFrame - lastHuntDbgFrame >= 60) {
        lastHuntDbgFrame = info.GameFrame;
        string hunterList;
        for(int sn : hunterFarmersSN) hunterList += to_string(sn) + " ";
        DebugText("[huntDBG] frame=" + to_string(info.GameFrame)
                  + " huntingPhase=" + to_string(huntingPhase)
                  + " nearestGazelleSN=" + to_string(nearestGazelleSN)
                  + " lastkill=(" + to_string(lastkillX) + "," + to_string(lastkillY) + ")"
                  + " hunters={" + hunterList + "}"
                  + " deadMeat=" + to_string(deadMeatResources.size()));
    }

    // 1. 无目标时重选最近活羚羊
    if(nearestGazelleSN == -1) {
        nearestGazelleSN = getNearestResource(RESOURCE_GAZELLE, centerX, centerY, 0);
        DebugText("[huntDBG] 重新选目标 nearestGazelleSN=" + to_string(nearestGazelleSN));
    }

    // 2. 校验/初始化猎物信息（必须在此完成，后续所有读取都依赖它）
    tagResource nearestGazelle;
    bool haveLiveTarget = false;
    if(nearestGazelleSN != -1) {
        for(auto& resource : info.resources) {
            if(resource.SN == nearestGazelleSN) {
                nearestGazelle = resource;
                haveLiveTarget = true;
                break;
            }
        }
        if(!haveLiveTarget) {
            DebugText("[huntDBG] 目标SN=" + to_string(nearestGazelleSN)
                      + " 已离开视野，清除目标等待重选");
            nearestGazelleSN = -1;
        }
    }

    // 无尸体可收判定（2026-10-08）：击杀队列与地图上未采完尸体(Blood<=0且Cnt>0)均无
    auto hasUncollectedCorpses = [&]() -> bool {
        if(!deadMeatResources.empty()) return true;
        for(auto& r : info.resources)
            if(r.Type == RESOURCE_GAZELLE && r.Blood <= 0 && r.Cnt > 0) return true;
        return false;
    };

    // 3. 无可见活猎物：有未收尸体才切收集；无尸体保持狩猎相位等猎物出现
    //    （旧版会把 huntingPhase 卡在 1 而无尸可收，猎人永久待命）
    if(!haveLiveTarget) {
        DebugText("[huntDBG] 无可见活猎物，尝试切收集阶段 deadMeat="
                  + to_string(deadMeatResources.size()));
        if(hasUncollectedCorpses()) {
            huntingPhase = 1;
        }
        return;
    }

    // 4. 有猎物：判断是否切收集阶段（猎物过远时）
    //    2026-10-08 修复三重门禁：①肉已够时代升级(≥800或已青铜) ②确有未收尸体
    //    ③猎物距上次击杀>10格。旧版只看距离——羚羊群受惊整群迁移十几格是常态，
    //    过早切收集导致仓库提前开建、肉不够升时代、尸体采完后 huntingPhase 卡 1。
    //    肉不足时无论多远都继续追猎，保证升级时代的肉量。
    double distToLastKill = euclidean_distance(nearestGazelle.DR, nearestGazelle.UR, lastkillX, lastkillY);
    bool meatSufficient = (info.civilizationStage >= CIVILIZATION_BRONZEAGE)
                          || info.Meat >= BUILDING_CENTER_UPGRADE_BRONZEAGE_FOOD;
    if(lastkillX >= 0 && meatSufficient && hasUncollectedCorpses()
       && distToLastKill > 10 * BLOCKSIDELENGTH) {
        DebugText("[huntDBG] 猎物过远(" + to_string(distToLastKill)
                  + ">" + to_string((int)(10 * BLOCKSIDELENGTH)) + ")且肉已足，切收集阶段");
        huntingPhase = 1;
        return;
    }

    // 5. 派发猎人攻击（此时 nearestGazelleSN 必为有效正数）
    for(auto& farmer: info.farmers) {
        if(hunterFarmersSN.count(farmer.SN) == 0) continue;
        if(farmerTask[farmer.SN] != nearestGazelleSN) {
            farmerTask[farmer.SN] = nearestGazelleSN;
            HumanAction(farmer.SN, nearestGazelleSN);
            DebugText("[huntDBG] 派 hunter SN=" + to_string(farmer.SN)
                      + " 攻击 gazelleSN=" + to_string(nearestGazelleSN)
                      + " @(" + to_string(nearestGazelle.BlockDR) + "," + to_string(nearestGazelle.BlockUR) + ")");
        }
    }

    // 6. 猎物死亡处理
    if(nearestGazelle.Blood <= 0) {
        DebugText("[huntDBG] 猎物死亡 SN=" + to_string(nearestGazelleSN)
                  + " 位置@(" + to_string(nearestGazelle.BlockDR) + "," + to_string(nearestGazelle.BlockUR) + ")");
        deadMeatResources.push_back(nearestGazelleSN);
        stockX += nearestGazelle.BlockDR;
        stockY += nearestGazelle.BlockUR;
        lastkillX = nearestGazelle.DR;
        lastkillY = nearestGazelle.UR;
        nearestGazelleSN = -1;
    }
}

bool UsrAI::isSingle(int BlockDR, int BlockUR) {
    if(treeCoverMap[BlockDR][BlockUR] > 1) return false;
    int emptyNeighborCnt = 0 ;
    for(int i = 0; i < 8; i++) {
        if(treeCoverMap[BlockDR + dx[i]][BlockUR + dy[i]] <= 1) {
            emptyNeighborCnt++;
        }
    }
    return emptyNeighborCnt >= 3;
}

// 设计目标：最稳妥、最不容易卡脚
//   1. 独占树机制：一村民一棵树，避免多人抢同一棵树导致路径堆叠与互相阻塞
//   2. 砍完自动释放：树 SN 从 info.resources 消失即视为砍完，立即从占用表清除
//   3. 卡脚阈值宽松：连续 60 帧（约 1 秒）IDLE 才判卡脚重派，避免误判村民往返
//      搬运途中短暂 IDLE（内核寻路重算期间的正常 IDLE）
//   4. per-farmer 15 帧节流：避免对同一村民每帧重复 HumanAction 抢占内核指令队列
//   5. 选最近的空闲树：曼哈顿距离最近，缩短村民往返搬运木材的路径
//   6. 组别过滤：phase2 只认 treeFarmersSN；phase1 跳过 builder 与已分组的
//      stone/repair/berry/hunter/gold 村民，避免 logging() 与 goldMining() 等同帧
//      对同一村民下发冲突 HumanAction
//   7. 一旦分配到树就保持，仅在砍完或卡脚时重派，最大限度减少指令抖动
//   8. 死亡村民的独占树立即释放，防止死锁导致后续村民找不到空闲树
//   9. 边缘树约束：只砍树林边缘上的树，避免砍穿中间导致内部树后续不可达
void UsrAI::logging() {
    static map<int,int> farmerTree;        // 农民 SN -> 独占树 SN
    static set<int> occupiedTrees;          // 已被占用的树 SN（含正在砍、刚砍完未释放）
    static map<int,int> idleSince;          // 农民 SN -> 连续 IDLE 起始帧（卡脚判定用）
    static map<int,int> lastOrderFrame;     // per-farmer 节流：避免对同一村民每帧重复 HumanAction
    static int lastDbgFrame = -10000;       // 调试输出节流
    static map<int,pair<int,int>> nudgeTarget;   // 卡脚解锁：SN -> 随机解锁目标点
    static map<int,int> nudgeStartFrame;         // 卡脚解锁：SN -> 解锁开始帧（超时兜底）
    const int ORDER_INTERVAL = 15;          // per-farmer 节流间隔
    const int STUCK_THRESHOLD = 60;         // 60 帧（1秒）IDLE 才判卡脚
    const int NUDGE_TIMEOUT = 180;          // 解锁移动超时（帧），超时后放弃等待直接重派

    // (1) 清理已消失的树（被砍完）：树 SN 不在 info.resources 即视为砍完
    set<int> liveTreeSNs;
    for(auto& r : info.resources)
        if(r.Type == RESOURCE_TREE) liveTreeSNs.insert(r.SN);
    for(auto it = occupiedTrees.begin(); it != occupiedTrees.end();) {
        if(liveTreeSNs.count(*it) == 0) it = occupiedTrees.erase(it);
        else ++it;
    }
    for(auto it = farmerTree.begin(); it != farmerTree.end();) {
        if(liveTreeSNs.count(it->second) == 0) it = farmerTree.erase(it);
        else ++it;
    }

    // (2) 收集所有已占用树的位置（用于候选树的最小间距判定）
    //   要求：任意两棵"正在被砍"的树之间切比雪夫距离 >= 1 格
    //   避免两个村民砍同一棵树（资源不重叠时本约束宽松）
    vector<pair<int,int>> occupiedPos;
    for(auto& r : info.resources) {
        if(r.Type != RESOURCE_TREE) continue;
        if(occupiedTrees.count(r.SN)) occupiedPos.push_back({r.BlockDR, r.BlockUR});
    }

    // (2.5) 收集所有树的位置，用于"边缘树"判定
    //   边缘树定义：8 邻位中至少有一格没有树（孤树自然算边缘）
    //   砍伐边缘树的好处：保持树林形状，避免从中间砍出洞导致内部树后续无法到达
    set<pair<int,int>> treePositions;
    for(auto& r : info.resources) {
        if(r.Type != RESOURCE_TREE) continue;
        treePositions.insert({r.BlockDR, r.BlockUR});
    }
    static const int EDGE_DX[] = {-1,-1,-1, 0, 0, 1, 1, 1};
    static const int EDGE_DY[] = {-1, 0, 1,-1, 1,-1, 0, 1};
    auto isEdgeTree = [&](int x, int y) -> bool {
        for(int i = 0; i < 8; i++) {
            if(treePositions.count({x + EDGE_DX[i], y + EDGE_DY[i]}) == 0) {
                return true;   // 至少一个邻位无树 → 边缘树
            }
        }
        return false;            // 8 邻位全是树 → 内部树
    };

    // (3) 选最近的空闲树：曼哈顿距离 + 跳过原卡点 + 距已占用树 ≥ 1 格 + 必须是边缘树
    //   useStockRef=true  → 以仓库/市镇中心为基准选最近（首次分配，缩短木材搬运路径）
    //   useStockRef=false → 以村民当前位置为基准选最近（卡脚重派，避免村民走远再次卡脚）
    auto findNearestFreeTree = [&](int fx, int fy, int skipSN, bool useStockRef) -> int {
        int bestSN = -1, bestDist = INT_MAX;
        for(auto& r : info.resources) {
            if(r.Type != RESOURCE_TREE) continue;
            if(r.SN == skipSN) continue;             // 跳过原卡点树
            if(occupiedTrees.count(r.SN)) continue;
            // 边缘约束：只砍树林边缘上的树（避免砍穿中间导致内部树不可达）
            if(!isEdgeTree(r.BlockDR, r.BlockUR)) continue;
            // 间距约束：到所有已占用树切比雪夫距离 >= 2 格
            bool tooClose = false;
            for(auto& op : occupiedPos) {
                if(max(abs(r.BlockDR - op.first), abs(r.BlockUR - op.second)) < 2) {
                    tooClose = true;
                    break;
                }
            }
            if(tooClose) continue;
            int d;
            if(useStockRef) {
                // 取该树到所有仓库/市镇中心的最小曼哈顿距离
                int minStockDist = INT_MAX;
                for(auto& b : info.buildings) {
                    if(b.Type == BUILDING_CENTER || b.Type == BUILDING_STOCK) {
                        minStockDist = min(minStockDist,
                            abs(r.BlockDR - b.BlockDR) + abs(r.BlockUR - b.BlockUR));
                    }
                }
                if(minStockDist == INT_MAX) continue;   // 地图上无仓库/中心
                d = minStockDist;
            } else {
                d = abs(r.BlockDR - fx) + abs(r.BlockUR - fy);
            }
            if(d < bestDist) { bestDist = d; bestSN = r.SN; }
        }
        return bestSN;
    };

    // (4) 遍历所有农民，状态机：
    //   - 新农民（无树分配）→ 立即分配
    //   - 原树砍完（farmerTree 已被 (1) 清理）→ 立即重派
    //   - 卡脚（连续 STUCK_THRESHOLD 帧 IDLE）→ 释放原树，跳过原树重派
    //   - 其它情况（WORKING/WALKING/往返途中短时 IDLE）→ 不干预
    // 节流改为 per-farmer：避免全局节流误伤"砍完树立即重派"的村民
    for(auto& farmer : info.farmers) {
        int sn = farmer.SN;
        int fx = farmer.BlockDR, fy = farmer.BlockUR;

        // 组别过滤：
        //   phase2（unifiedAssignDone）：只认 treeFarmersSN，保障房屋/军队/科技木材供应
        //   phase1：跳过 builderSN 与已分组的 stone/repair/berry/hunter/gold 村民，
        //           避免与 goldMining() 等同帧对同一村民下发冲突指令
        if(unifiedAssignDone) {
            if(treeFarmersSN.count(sn) == 0) continue;
        } else {
            if(sn == builderSN) continue;
            // 开局待命村民不派工（等配对打猎）；配对新村民正在走向待命村民，也不抢派
            if(sn == waitingHunterSN || sn == pairingFarmerSN) continue;
            if(stoneFarmersSN.count(sn) || repairFarmersSN.count(sn)
               || berryFarmersSN.count(sn) || hunterFarmersSN.count(sn)
               || goldFarmersSN.count(sn)) continue;
        }

        // 建筑工专项协调（phase2 建筑工备料砍树期间）：
        //  1) 正在打地基/修建筑（WORKING 且目标是我方建筑）：释放砍树占位并完全不干预，
        //     绝不做卡脚 nudge / 重新派树，等他完工；
        //  2) IDLE 且 buildTasks 队首任务资源+落点已就绪：保持 IDLE 留给本帧稍后的
        //     assignBuilding() 直接 HumanBuild，否则这里抢先派树会把下单窗口挤掉；
        //     资源不足或无合法点时照常派树备料。
        if(sn == builderSN) {
            if(farmer.NowState == HUMAN_STATE_WORKING && farmer.WorkObjectSN > 0) {
                bool onBuilding = false;
                for(auto& b : info.buildings)
                    if(b.SN == farmer.WorkObjectSN) { onBuilding = true; break; }
                if(onBuilding) {
                    auto ptIt = farmerTree.find(sn);
                    if(ptIt != farmerTree.end()) {
                        occupiedTrees.erase(ptIt->second);
                        farmerTree.erase(ptIt);
                    }
                    idleSince.erase(sn);
                    continue;
                }
            }
            if(farmer.NowState == HUMAN_STATE_IDLE && !buildTasks.empty()) {
                auto& bt = buildTasks.front();
                if(bt.BlockDR <= MAP_SIZE && checkResource(bt.Type)) {
                    pair<int,int> pl = legalPlaceAround(bt.Type, bt.BlockDR, bt.BlockUR);
                    if(pl.first == -1 && bt.Type == BUILDING_HOME
                       && (bt.BlockDR != centerX || bt.BlockUR != centerY))
                        pl = legalPlaceAround(bt.Type, centerX, centerY);
                    if(pl.first != -1) continue;   // 就绪：不抢派，等 assignBuilding 下单
                }
            }
        }

        // 更新 IDLE 起始帧
        if(farmer.NowState == HUMAN_STATE_IDLE) {
            if(idleSince.count(sn) == 0) idleSince[sn] = info.GameFrame;
        } else {
            idleSince.erase(sn);
        }

        bool needAssign = false;
        bool firstAssign = false;   // true=首次分配(含砍完重派)，false=卡脚重派
        int skipSN = -1;            // 卡脚重派时跳过的原树 SN
        // 情况A: 未分配树（含树砍完已被清理）→ 首次分配
        if(farmerTree.count(sn) == 0) {
            needAssign = true;
            firstAssign = true;
        }
        // 情况B: 卡脚 - 连续 STUCK_THRESHOLD 帧 IDLE
        else if(idleSince.count(sn) &&
                info.GameFrame - idleSince[sn] >= STUCK_THRESHOLD) {
            needAssign = true;
            skipSN = farmerTree[sn];            // 记下原卡点树，选新树时跳过
            occupiedTrees.erase(farmerTree[sn]);
            farmerTree.erase(sn);
            idleSince.erase(sn);

            // 卡脚解锁：先向周围 3 格内的随机空闲格 HumanMove，
            // 到达解锁点后才安排继续工作（防止原地反复重派仍卡在同一障碍）
            int tx = -1, ty = -1;
            for(int t = 0; t < 8; t++) {
                int ndx = (int)((info.GameFrame * 31 + sn * 17 + t * 5) % 7) - 3;   // [-3,3] 伪随机
                int ndy = (int)((info.GameFrame * 13 + sn * 29 + t * 7) % 7) - 3;
                if(ndx == 0 && ndy == 0) continue;
                int cx = fx + ndx, cy = fy + ndy;
                if(cx < 0 || cx >= MAP_SIZE || cy < 0 || cy >= MAP_SIZE) continue;
                if(gameMap[cx][cy] != 0) continue;   // 目标格被占用，换一个
                tx = cx; ty = cy;
                break;
            }
            if(tx != -1) {
                nudgeTarget[sn] = make_pair(tx, ty);
                nudgeStartFrame[sn] = info.GameFrame;
                lastOrderFrame[sn] = info.GameFrame;
                HumanMove(sn, tx * BLOCKSIDELENGTH, ty * BLOCKSIDELENGTH);   // 内核按像素坐标解释
                continue;   // 本轮不下发砍树指令，等走到解锁点
            }
            // 周围 3 格无空地：退化为直接重派（needAssign 已置位）
        }

        // 解锁移动进行中：暂不下发工作指令，等村民到达解锁点或超时后再安排
        if(nudgeTarget.count(sn)) {
            pair<int,int> nt = nudgeTarget[sn];
            bool arrived = (abs(fx - nt.first) <= 1 && abs(fy - nt.second) <= 1);
            bool timeout = (info.GameFrame - nudgeStartFrame[sn] >= NUDGE_TIMEOUT);
            if(arrived || timeout) {
                nudgeTarget.erase(sn);
                nudgeStartFrame.erase(sn);
            } else {
                continue;   // 正在走向解锁点，不干预
            }
        }

        if(!needAssign) continue;

        // per-farmer 节流：同一村民 15 帧内不重复下发 HumanAction
        if(info.GameFrame - lastOrderFrame[sn] < ORDER_INTERVAL) continue;

        int treeSN = findNearestFreeTree(fx, fy, skipSN, firstAssign);
        if(treeSN == -1) {
            // 找不到空闲树：60 帧输出一次诊断，避免刷屏
            if(info.GameFrame - lastDbgFrame >= 60) {
                lastDbgFrame = info.GameFrame;
                int idleCnt = 0;
                for(auto& f : info.farmers)
                    if(f.NowState == HUMAN_STATE_IDLE) idleCnt++;
                DBG_OFF("[logDBG] no free tree for farmer " + to_string(sn)
                          + " idleTotal=" + to_string(idleCnt)
                          + " frame=" + to_string(info.GameFrame));
            }
            continue;
        }
        farmerTree[sn] = treeSN;
        occupiedTrees.insert(treeSN);
        lastOrderFrame[sn] = info.GameFrame;
        HumanAction(sn, treeSN);
    }
}

// 开局配对打猎：待命村民原地不动，市镇中心生产出的首个新村民走到他身边，
// 两人会合后一起进猎人组（hunting()/collecting() 模块不变，统一管理）
void UsrAI::pairFirstHunters() {
    if(waitingHunterSN == -1) return;   // 无待命村民（已配对完成或开局无人可留）

    // (0) 待命村民是否存活；死亡则取消配对，配对新村民交回常规分配
    bool waiterAlive = false;
    int wx = 0, wy = 0;
    for(auto& f : info.farmers) {
        if(f.SN == waitingHunterSN) { waiterAlive = true; wx = f.BlockDR; wy = f.BlockUR; break; }
    }
    if(!waiterAlive) {
        DebugText("[huntDBG] 待命村民 " + to_string(waitingHunterSN)
                  + " 阵亡，配对中止 frame=" + to_string(info.GameFrame));
        waitingHunterSN = -1;
        pairingFarmerSN = -1;
        return;
    }

    // (1) 校验当前配对新村民；失效（死亡/被其它模块收编）则重新挑选未被标记的村民
    bool pairingValid = false;
    int px = 0, py = 0;
    if(pairingFarmerSN != -1) {
        for(auto& f : info.farmers) {
            if(f.SN != pairingFarmerSN) continue;
            bool claimed = berryFarmersSN.count(f.SN) || hunterFarmersSN.count(f.SN)
                        || stoneFarmersSN.count(f.SN) || repairFarmersSN.count(f.SN)
                        || goldFarmersSN.count(f.SN) || currentFarmersSN.count(f.SN);
            if(!claimed) { pairingValid = true; px = f.BlockDR; py = f.BlockUR; }
            break;
        }
    }
    if(!pairingValid) {
        if(pairingFarmerSN != -1) {
            DebugText("[huntDBG] 配对候选 " + to_string(pairingFarmerSN)
                      + " 失效（阵亡/被其它模块收编），重新挑选 frame=" + to_string(info.GameFrame));
        }
        pairingFarmerSN = -1;
        for(auto& f : info.farmers) {
            int sn = f.SN;
            if(sn == builderSN || sn == waitingHunterSN) continue;
            if(berryFarmersSN.count(sn) || hunterFarmersSN.count(sn)) continue;
            if(stoneFarmersSN.count(sn) || repairFarmersSN.count(sn)) continue;
            if(goldFarmersSN.count(sn) || currentFarmersSN.count(sn)) continue;
            pairingFarmerSN = sn;
            px = f.BlockDR; py = f.BlockUR;
            DebugText("[huntDBG] 选定配对新村民 " + to_string(sn)
                      + " -> 待命村民 " + to_string(waitingHunterSN)
                      + " @(" + to_string(px) + "," + to_string(py) + ") vs (" + to_string(wx) + "," + to_string(wy) + ")");
            break;
        }
    }

    // ===== 配对过程调试（2026-10-08）：每60帧输出中间状态 =====
    // 内容：待命/配对村民 SN、位置、状态、切比雪夫距离（<=1 即会合完成）
    {
        static int pairDbgFrame = -10000;
        if(info.GameFrame - pairDbgFrame >= 60) {
            pairDbgFrame = info.GameFrame;
            int wState = -1, pState = -1;
            for(auto& f : info.farmers) {
                if(f.SN == waitingHunterSN) wState = f.NowState;
                if(f.SN == pairingFarmerSN) pState = f.NowState;
            }
            int cheb = (pairingFarmerSN != -1) ? max(abs(px - wx), abs(py - wy)) : -1;
            DebugText("[huntDBG配对] frame=" + to_string(info.GameFrame)
                      + " 待命村民=" + to_string(waitingHunterSN)
                      + "@(" + to_string(wx) + "," + to_string(wy) + ")st" + to_string(wState)
                      + " 配对候选=" + (pairingFarmerSN != -1
                          ? to_string(pairingFarmerSN) + "@(" + to_string(px) + "," + to_string(py)
                            + ")st" + to_string(pState)
                          : string("无(市镇中心未产出新村民)"))
                      + " 切比雪夫距离=" + to_string(cheb) + "(<=1会合)");
        }
    }
    if(pairingFarmerSN == -1) return;   // 市镇中心尚未产出新村民

    // (2) 距离>1 时命令新村民走向待命村民（20 帧节流，防止重复 HumanMove）；
    //     注意 INS_HUMANMOVE 内核按像素坐标解释，块坐标需乘 BLOCKSIDELENGTH；
    //     会合（切比雪夫距离<=1，含斜对角相邻）后两人一起开始打猎
    //     ——不能用曼哈顿距离：目标格被待命村民占据时内核会把新村民停在斜对角，
    //     斜对角曼哈顿=2 永远>1，导致配对永远不完成、两人原地罚站
    if(max(abs(px - wx), abs(py - wy)) > 1) {
        static int lastMoveFrame = -10000;
        if(info.GameFrame - lastMoveFrame >= 20) {
            lastMoveFrame = info.GameFrame;
            HumanMove(pairingFarmerSN, wx * BLOCKSIDELENGTH, wy * BLOCKSIDELENGTH);
            DebugText("[huntDBG] 配对移动中 " + to_string(pairingFarmerSN)
                      + " (" + to_string(px) + "," + to_string(py) + ") -> ("
                      + to_string(wx) + "," + to_string(wy) + ")");
        }
    } else {
        hunterFarmersSN.insert(pairingFarmerSN);
        hunterFarmersSN.insert(waitingHunterSN);
        DebugText("[huntDBG] 配对完成: " + to_string(pairingFarmerSN)
                  + " + " + to_string(waitingHunterSN) + " -> hunting");
        pairingFarmerSN = -1;
        waitingHunterSN = -1;
    }
}

void UsrAI::gamePhase1() {
    static bool buildingScheduled = false;
    static bool homeBuilt = false;

    // 自建开局设施（2房屋+1仓库）全部建成后，一次性排入军营/市场/马厩/靶场（提前于800肉门槛）
    // 统计已建成（满血）的房屋/仓库总数，扣除开局赠送的 2 房屋 + 1 仓库，只认自建部分
    static bool milBuildingsScheduled = false;
    if(!milBuildingsScheduled) {
        int homeCnt = 0, stockCnt = 0;
        for(auto& b : info.buildings) {
            if(b.Type == BUILDING_HOME && b.Blood == b.MaxBlood) homeCnt++;
            else if(b.Type == BUILDING_STOCK && b.Blood == b.MaxBlood) stockCnt++;
        }
        if(homeCnt - 2 >= 2 && stockCnt - 1 >= 1) {
            buildTasks.push_back({BUILDING_ARMYCAMP, centerX, centerY, {}});
            buildTasks.push_back({BUILDING_MARKET, centerX, centerY, {}});
            buildTasks.push_back({BUILDING_STABLE, centerX, centerY, {}});
            buildTasks.push_back({BUILDING_RANGE, centerX, centerY, {}});
            milBuildingsScheduled = true;
            DBG_OFF("[buildDBG] opening facilities done, queued ARMYCAMP/MARKET/STABLE/RANGE frame="
                      + to_string(info.GameFrame));
        }
    }

    // 新生村民分配（伐木:采肉 = 2:1）：每 3 人中前 2 人进伐木组（logging() 派树），
    // 第 3 人进猎人组——huntingPhase==0 时 hunting() 派其攻击最近活羚羊，
    // huntingPhase==1 时 collecting() 派其收集羚羊肉（羚羊肉采集体系）。
    // 配对候选由 processData 里的 pairFirstHunters 先行锁定（在 assignFarmer 之前执行），
    // 此处跳过待命/配对中村民即可，无抢人竞争。
    // 注意：本处调试用裸 DebugText 输出（DBG_OFF 是空操作，不产生任何输出）
    static int phase1NewFarmerIdx = 0;
    for(auto& farmer : info.farmers) {
        if(farmer.SN == builderSN) continue;
        if(farmer.SN == waitingHunterSN || farmer.SN == pairingFarmerSN) continue;
        if(stoneFarmersSN.count(farmer.SN) || repairFarmersSN.count(farmer.SN)
           || berryFarmersSN.count(farmer.SN) || hunterFarmersSN.count(farmer.SN)
           || goldFarmersSN.count(farmer.SN) || treeFarmersSN.count(farmer.SN)
           || currentFarmersSN.count(farmer.SN)) continue;
        if(phase1NewFarmerIdx % 3 == 2) {
            hunterFarmersSN.insert(farmer.SN);  // 第 3 人 → 羚羊肉采集（hunting/collecting 接管）
            DebugText("[assignDBG] p1 farmer " + to_string(farmer.SN) + " -> MEAT #"
                      + to_string(phase1NewFarmerIdx) + " frame=" + to_string(info.GameFrame));
        } else {
            treeFarmersSN.insert(farmer.SN);    // 前 2 人 → 伐木，logging() 派树
            DebugText("[assignDBG] p1 farmer " + to_string(farmer.SN) + " -> TREE #"
                      + to_string(phase1NewFarmerIdx) + " frame=" + to_string(info.GameFrame));
        }
        phase1NewFarmerIdx++;
    }
    // 60 帧节流的组员快照：核查"全员砍树"类分配异常时打开游戏内文本即可
    static int lastAssignDbgFrame = -10000;
    if(info.GameFrame - lastAssignDbgFrame > 60) {
        lastAssignDbgFrame = info.GameFrame;
        string treeList, meatList;
        for(auto& f : info.farmers) {
            if(treeFarmersSN.count(f.SN)) treeList += to_string(f.SN) + " ";
            if(hunterFarmersSN.count(f.SN)) meatList += to_string(f.SN) + " ";
        }
        DebugText("[assignDBG] frame=" + to_string(info.GameFrame)
                  + " idx=" + to_string(phase1NewFarmerIdx)
                  + " tree={" + treeList + "} meat={" + meatList + "}");
    }

    if(info.Meat < 800){
        if(!homeBuilt) {
            homeBuilt = true;
            buildTasks.push_back({BUILDING_HOME, homeX, homeY, {}});
            buildTasks.push_back({BUILDING_HOME, homeX, homeY, {}});
            DBG_OFF("[homeDBG] phase1 initial home pushed @(" + to_string(homeX) + "," + to_string(homeY)
                      + ") frame=" + to_string(info.GameFrame));
        }
        // 维护浆果村民（开局 6 人；新村民已按 2:1 分配，第 3 人进猎人组而非浆果组）
        berryCollecting();
        if(huntingPhase == 0)
            hunting();
        else
            collecting();
        // 新村民已在函数开头按 伐木:采肉=2:1 分组；logging() 继续处理伐木组的
        // 派树/重派/卡脚，并兜底收编浆果枯竭后释放的村民
        logging();
    } else {
        if(!buildingScheduled) {
            buildTasks.push_back({BUILDING_COLLAGE, centerX, centerY, {}});
            buildTasks.push_back({BUILDING_RANGE, centerX, centerY, {}});
            //buildTasks.push_back({BUILDING_MARKET_GOLD_UPGRADE, marketSN, marketSN, {}});
            buildingScheduled = true;
        }

        // fixingArrowTower 已在 processData 统一调用，此处不再重复
        // 浆果/羚羊农民持续采集食物，采完空闲后由 logging() 统一收编伐木
        berryCollecting();                 // 维护浆果村民（含枯竭后转伐木组）
        if(huntingPhase == 0) hunting(); else collecting();
        // 羚羊/肉采集完毕（无活羚羊）时，释放猎人，交回 logging() 统一分配
        // （同步清掉 collecting() 塞入 currentFarmersSN 的标记，防止卡在耕作组无人认领）
        bool liveGazelle = false;
        for(auto& r : info.resources)
            if(r.Type == RESOURCE_GAZELLE && r.Cnt > 0) { liveGazelle = true; break; }
        if(!liveGazelle && deadMeatResources.empty() && huntingPhase == 0) {
            // 羚羊尸体收集完毕：全部猎人转伐木组（不进采金组），在伐木组中等待
            // farming() 抽调建田（farming 抽人顺序：采金组优先，伐木组回退）；
            // 显式入组可避免被新生村民交替分配（偶伐木/奇采金）或转采金分支抢走
            for(int sn : hunterFarmersSN) {
                currentFarmersSN.erase(sn);
                treeFarmersSN.insert(sn);
            }
            hunterFarmersSN.clear();
            corpseHelperSNs.clear();
        }
        logging();
        updateStage();
    }
}


void UsrAI::farming() {
    const int FARM_SLOTS = 8;   // farmDx/farmDy 数组长度
    if(info.Wood < BUILD_FARM_WOOD || farmNum >= FARM_SLOTS) return;

    pair<int,int> nextFarmPosition;

    int nextFarmX, nextFarmY;
    nextFarmPosition = legalPlaceAround(BUILDING_FARM, granaryX + farmDx[farmNum], granaryY + farmDy[farmNum]);
    nextFarmX = nextFarmPosition.first;
    nextFarmY = nextFarmPosition.second;
    if(nextFarmX == -1) return;   // 该预设点附近无合法地块，等下帧/木材变化后重试

    DBG_OFF("nextFarmX = " + to_string(nextFarmX) + ", nextFarmY = " + to_string(nextFarmY));
    // 抽人策略：优先从采金组（goldFarmersSN）抢人建田，建完田该村民自动开始耕作
    // （内核建完即驻田工作）；采金组无可用人选时回退伐木组。
    // 跳过维修工——他们只是临时借调在采金组，须保持可被召回修塔；
    // 跳过 builder——建造任务专职人员不抽调。
    auto pullFarmBuilder = [&]() -> int {
        for(int sn : goldFarmersSN) {
            if(sn == builderSN || repairFarmersSN.count(sn)) continue;
            goldFarmersSN.erase(sn);              // 移出采金组，避免 goldMining() 再派他去采金
            currentFarmersSN.insert(sn);
            return sn;
        }
        if(!treeFarmersSN.empty()) {
            // 必须跳过 builderSN：建筑工在房屋补建期会被挂进伐木组备料，
            // 且他是 SN 最小的村民（set::begin 必中），不跳过会被反复抽去放农田占位符
            int picked = -1;
            for(int sn : treeFarmersSN) {
                if(sn == builderSN) continue;
                picked = sn;
                break;
            }
            if(picked != -1) {
                treeFarmersSN.erase(picked);      // 移出伐木组，避免 logging() 再派他去伐木
                currentFarmersSN.insert(picked);
                return picked;
            }
        }
        return -1;
    };

    int sn = pullFarmBuilder();
    if(sn != -1) {
        farmerTask[sn] = BUILDING_FARM;
        HumanBuild(sn, BUILDING_FARM, nextFarmX, nextFarmY);
        farmerFarmPos[sn] = {nextFarmX, nextFarmY};   // 记录该村民负责的农田
        farmNum++;
        return;
    }

    if(info.Wood < BUILD_FARM_WOOD || farmNum >= FARM_SLOTS) return;
    nextFarmPosition = legalPlaceAround(BUILDING_FARM, granaryX + farmDx[farmNum], granaryY + farmDy[farmNum]);
    nextFarmX = nextFarmPosition.first;
    nextFarmY = nextFarmPosition.second;
    if(nextFarmX == -1) return;

    // 第二块同帧农田：同样按 采金组优先、伐木组回退 抽人
    int sn2 = pullFarmBuilder();
    if(sn2 != -1) {
        HumanBuild(sn2, BUILDING_FARM, nextFarmX, nextFarmY);
        farmerTask[sn2] = 2;
        farmerFarmPos[sn2] = {nextFarmX, nextFarmY};   // 记录该村民负责的农田
        farmNum++;
    }
}

// 耕作村民卡脚兜底：每25帧(1s)轮询耕作组成员，若处于 IDLE（被地形/其它单位卡住
// 导致建田后没驻田工作，或工作关系异常中断），优先让他回到自己被派去建的那块农田
// （farmerFarmPos 记录），无记录或该田已消失时才退化为最近的已建成农田
void UsrAI::farmingIdlePoll() {
    static int lastPollFrame = -100;
    if(info.GameFrame - lastPollFrame < 25) return;
    if(currentFarmersSN.empty()) return;
    lastPollFrame = info.GameFrame;

    for(int sn : currentFarmersSN) {
        tagFarmer* f = nullptr;
        for(auto& farmer : info.farmers) {
            if(farmer.SN == sn) { f = &farmer; break; }
        }
        if(!f || f->NowState != HUMAN_STATE_IDLE) continue;

        int targetFarmSN = -1;
        bool targetUnfinished = false;

        // 1. 优先找该村民一开始去建的农田（按记录的左上角块坐标，3×3包含匹配）。
        //    半成品地基（Percent<100）优先——HumanAction 指向建筑 SN 会加入建造，
        //    修复"只放了占位符、无人实际建造"；地基不存在时才回成品田耕作
        auto posIt = farmerFarmPos.find(sn);
        if(posIt != farmerFarmPos.end()) {
            int fx = posIt->second.first, fy = posIt->second.second;
            int completedMatch = -1;
            for(auto& b : info.buildings) {
                if(b.Type != BUILDING_FARM || b.Blood <= 0) continue;
                if(fx >= b.BlockDR && fx < b.BlockDR + 3
                   && fy >= b.BlockUR && fy < b.BlockUR + 3) {
                    if(b.Percent < 100) { targetFarmSN = b.SN; targetUnfinished = true; break; }
                    if(completedMatch == -1) completedMatch = b.SN;
                }
            }
            if(targetFarmSN == -1) targetFarmSN = completedMatch;
        }

        // 2. 退化：记录丢失或原田已不存在（被摧毁）→ 最近的已建成农田
        if(targetFarmSN == -1) {
            int bestDist = INT_MAX;
            for(auto& b : info.buildings) {
                if(b.Type != BUILDING_FARM || b.Blood <= 0 || b.Percent < 100) continue;
                int d = manhattan_distance(b.BlockDR, b.BlockUR, f->BlockDR, f->BlockUR);
                if(d < bestDist) { bestDist = d; targetFarmSN = b.SN; }
            }
        }

        if(targetFarmSN != -1) {
            HumanAction(sn, targetFarmSN);
            DebugText("[farmPoll] 空闲村民 " + to_string(sn)
                      + (targetUnfinished ? " 继续建造半成品农田 farmSN=" : " 回到原农田 farmSN=")
                      + to_string(targetFarmSN)
                      + " frame=" + to_string(info.GameFrame));
        }
    }
}

// gamePhase2 统一分配（仅执行一次）：
// 1) 伐木工(treeFarmersSN)保留，持续供应房屋/军队/科技的木材需求；
// 2) 所有 repairFarmer 和 stoneFarmer 全部归入 goldMiner（goldFarmersSN）；
// 3) 之后新村民一律进 goldFarmersSN 挖金矿；
// 4) farming 每建一块农田时，从 treeFarmersSN 临时抽调一人转入耕作组，
//    建造者建完田后自动开始耕作。
void UsrAI::unifiedAssign() {
    if(unifiedAssignDone) return;
    unifiedAssignDone = true;

    // 收集进入 gamePhase2 前的"伐木工"快照：所有不属于任何已知工种、非 builder 的在册村民
    // 浆果/猎人组此时可能仍有成员（浆果未枯竭/羚羊尸体未收完，由各自释放逻辑交回伐木），
    // 因此跳过 berry/hunter/stone/repair 标记，未被标记的村民即当前伐木工（treeFarmer）
    for(auto& farmer : info.farmers) {
        if(farmer.SN == builderSN) continue;
        if(berryFarmersSN.count(farmer.SN) || hunterFarmersSN.count(farmer.SN)) continue;
        if(stoneFarmersSN.count(farmer.SN) || repairFarmersSN.count(farmer.SN)) continue;
        treeFarmersSN.insert(farmer.SN);   // 保留伐木组，不删
    }

    // 采石组 → 全部归入采金组（新版 resourceSwitch 不再产采石工，此段仅在旧存档状态生效）
    for(int sn : stoneFarmersSN)  goldFarmersSN.insert(sn);
    stoneFarmersSN.clear();
    // 维修组保留为常设工种（repairFarmersSN 不清空）：敌人在视野内时随时可修复箭塔；
    // 空闲期（无敌人且无可修箭塔）由 gamePhase2 临时借调去挖金（SN 进出 goldFarmersSN）
}

void UsrAI::goldMining() {
    static int first = 1;
    static int curIndex = 0;
    if(first) {
        bestCluster = findBestGoldCluster();
        if(bestCluster.sns.empty()) return;
        goldClusterReady = true;   // 通知 gamePhase2：farming 启动判定可用簇中心定位金矿旁仓库
        // 仅当金矿簇中心到最近仓库(中心/仓库)的曼哈顿距离 > 8 时，才在簇旁建新仓库，方便卸矿
        int minDist = INT_MAX;
        for(auto& building : info.buildings) {
            if(building.Type == BUILDING_CENTER || building.Type == BUILDING_STOCK) {
                minDist = min(minDist, manhattan_distance(building.BlockDR, building.BlockUR,
                                                          bestCluster.centerX, bestCluster.centerY));
            }
        }
        if(minDist > 8)
            buildTasks.push_back({BUILDING_STOCK, bestCluster.centerX, bestCluster.centerY, {} });
        first = 0;
    }
    // 采金组按 goldFarmersSN 维护（统一分配后的唯一采集主力），仿 stoneMining 模式：
    // 首次进入立即指派，之后仅在村民卡脚(连续两帧空闲)时重新指派
    if(goldFarmersSN.empty()) return;

    static bool flag = false;
    static map<int,int> farmerState;

    priority_queue<pair<int,int>, vector<pair<int,int>>, greater<pair<int,int>>> candGoldSNs;
    const int INF = 1e9;
    for(auto& r : info.resources) {
        if(r.Type != RESOURCE_GOLD || r.Cnt <= 0) continue;
        int minDist = INF;
        for(auto& b : info.buildings) {
            if(b.Type == BUILDING_CENTER || b.Type == BUILDING_STOCK) {
                minDist = min(minDist, manhattan_distance(b.BlockDR, b.BlockUR, r.BlockDR, r.BlockUR));
            }
        }
        if(minDist != INF) candGoldSNs.push(make_pair(minDist, r.SN));
    }
    if(candGoldSNs.empty()) return;

    for(auto& farmer : info.farmers) {
        if(goldFarmersSN.count(farmer.SN) == 0) continue;
        if(!flag || (farmerState[farmer.SN] == HUMAN_STATE_IDLE && farmer.NowState == HUMAN_STATE_IDLE)) {
            if(candGoldSNs.empty()) break;
            HumanAction(farmer.SN, candGoldSNs.top().second);
            candGoldSNs.pop();
        }
        farmerState[farmer.SN] = farmer.NowState;
    }

    flag = true;
}

void UsrAI::createArmy1() {
    // 反攻扩军目标：方阵兵 10、复合弓兵 18（按 info.armies 实际存活数统计，
    // 不用静态计数器——战损后会自动补员）。第二阶段毕业后进入反攻继续补到上限
    const int maxHopliteNum = 10;
    const int maxCompositeBowNum = 18;
    int hopliteNum = 0, compositeBowNum = 0;
    for(auto& army : info.armies) {
        if(army.Sort == AT_HOPLITE) hopliteNum++;
        else if(army.Sort == AT_COMPOSITE_BOWMAN) compositeBowNum++;
    }
    static bool firstBowmanDone = false;   // 升到铜器时代时立即训练一个弓箭手（一次性）
    static bool firstChariotDone = false;  // 车轮科技完成后立即训练一个四马战车（一次性）
    for(auto& building : info.buildings) {
        if(building.Type == BUILDING_RANGE && building.Project == ACT_NULL && info.Human_Num < info.Human_MaxNum) {
            // 铜器时代达成：立即造一个弓箭手（40食物+20木材，无需科技）
            if(!firstBowmanDone && info.civilizationStage >= CIVILIZATION_BRONZEAGE
               && info.Meat >= BUILDING_RANGE_CREATE_BOWMAN_FOOD
               && info.Wood >= BUILDING_RANGE_CREATE_BOWMAN_WOOD) {
                BuildingAction(building.SN, BUILDING_RANGE_CREATE_BOWMAN);
                firstBowmanDone = true;
                continue;
            }
            // 训练复合弓兵（40食物+20黄金）直到存活数达 18
            if(compositeBowDone && compositeBowNum < maxCompositeBowNum
               && info.Meat >= BUILDING_RANGE_CREATE_COMPOSITE_BOWMAN_FOOD
               && info.Gold >= BUILDING_RANGE_CREATE_COMPOSITE_BOWMAN_GOLD) {
                BuildingAction(building.SN, BUILDING_RANGE_CREATE_COMPOSITE_BOWMAN);
                compositeBowNum++;
            }
        }
    }
    for(auto& building : info.buildings) {
        if(building.Type == BUILDING_COLLAGE && building.Project == ACT_NULL && info.Meat >= BUILDING_COLLAGE_CREATE_HOPLITE_FOOD
            && info.Gold >= BUILDING_COLLAGE_CREATE_HOPLITE_GOLD && hopliteNum < maxHopliteNum && info.Human_Num < info.Human_MaxNum) {
            BuildingAction(building.SN, BUILDING_COLLAGE_CREATE_HOPLITE);
            hopliteNum++;
            break;
        }
    }
    // 车轮科技完成后，马厩立即造一个四马战车（一次性）
    for(auto& building : info.buildings) {
        if(building.Type == BUILDING_STABLE && building.Project == ACT_NULL && !firstChariotDone
           && wheelDone && info.Human_Num < info.Human_MaxNum) {
            BuildingAction(building.SN, BUILDING_STABLE_CREATE_CHARIOT);
            firstChariotDone = true;
            break;
        }
    }
}

void UsrAI::Defense() {
    // 第一、二阶段防守：士兵优先攻击离祭司最近的敌人（保护祭司不被战车弓兵猎杀）
    // 祭司实时位置从 info.armies 取（全局 priestX/priestY 只在 init 首帧赋值，不随移动更新）
    int priestBX = -1, priestBY = -1;
    for(auto& army : info.armies) {
        if(army.SN == priestSN) { priestBX = army.BlockDR; priestBY = army.BlockUR; break; }
    }
    if(priestBX < 0) return;    // 祭司阵亡则不干预

    for(auto& army : info.armies) {
        if(army.SN == priestSN || army.NowState != HUMAN_STATE_IDLE) continue;
        int bestSN = -1, bestTier = 3, bestDist = INT_MAX;
        // 四马战车优先攻击战车弓兵，其次复合弓兵，最后按离祭司最近
        for(auto& enemy : info.enemy_armies) {
            if(enemy.Blood <= 0) continue;
            int tier = 3;   // 默认最低优先级
            if(army.Sort == AT_CHARIOT) {
                if(enemy.Sort == AT_CHARIOT_ARCHER) tier = 1;       // 战车弓兵最高优先
                else if(enemy.Sort == AT_COMPOSITE_BOWMAN) tier = 2; // 复合弓兵次之
            }
            int distance = manhattan_distance(enemy.BlockDR, enemy.BlockUR, priestBX, priestBY);
            if(tier < bestTier || (tier == bestTier && distance < bestDist)) {
                bestTier = tier;
                bestDist = distance;
                bestSN = enemy.SN;
            }
        }
        if(bestSN == -1) return;
        HumanAction(army.SN, bestSN);
    }
}

void UsrAI::fixingArrowTower() {
    // 选择血量最低的受损箭塔（需求5：血量最低优先维修）
    // 维修者：仅 repairFarmersSN（14000帧后从采石组二次分配而来），builder 专注建造建筑
    static bool flag = false;
    static size_t lastRepairCount = 0;   // 上帧维修组人数，用于检测新增维修者后重新强制分配
    int minBlood = INT_MAX;
    int targetSN = -1;

    if(info.Stone < 50) {
        static int dbgStoneFrame = -10000;   // 调试：石料不足导致的提前返回
        if(info.GameFrame - dbgStoneFrame >= 50 && !repairFarmersSN.empty()) {
            dbgStoneFrame = info.GameFrame;
            DBG_OFF("[repairDBG] fixingArrowTower skipped: stone=" + to_string(info.Stone) + " < 50");
        }
        return;
    }

    for(auto& building : info.buildings) {
        if(building.Type == BUILDING_ARROWTOWER && building.Blood < building.MaxBlood) {
            if(building.Blood < minBlood) {
                minBlood = building.Blood;
                targetSN = building.SN;
            }
        }
    }

    if(targetSN == -1) return;

    // 维修组人数变化时（如14000帧二次分配新增4人），重置flag以强制立即分配所有维修者
    if(repairFarmersSN.size() != lastRepairCount) {
        flag = false;
        lastRepairCount = repairFarmersSN.size();
    }

    // 敌人在场时强制重派（每 50 帧一次）：打断借调期间遗留的挖金动作，
    // 否则从金矿召回的维修工 state=2（工作中）永远不满足 IDLE 条件，拿不到修复指令
    static int forceFrame = -10000;
    bool enemyPresent = !info.enemy_armies.empty();
    bool forceReassign = false;
    if(enemyPresent && info.GameFrame - forceFrame >= 50) {
        forceFrame = info.GameFrame;
        forceReassign = true;
    }

    // 首次分配立即切换；之后仅派空闲的维修者，不打断正在建造/采集的人（敌人在场时强制重派例外）
    for(auto& farmer : info.farmers) {
        if(repairFarmersSN.count(farmer.SN) == 0) continue;
        if(!flag || farmer.NowState == HUMAN_STATE_IDLE || forceReassign) {
            // 调试：记录指令实际下发（目标塔 SN + 农民 SN + 状态）
            DBG_OFF("[repairDBG] fix order: farmer=" + to_string(farmer.SN)
                      + " -> tower=" + to_string(targetSN)
                      + " state=" + to_string(farmer.NowState)
                      + (forceReassign ? " FORCED" : "")
                      + " frame=" + to_string(info.GameFrame));
            HumanAction(farmer.SN, targetSN);
        }
    }

    flag = true;
}

void UsrAI::gamePhase2() {
    static int collageBuilt = 1;
    static int farmBuildingPhase = 1;
    static int farmerInitialised = 0;
    static int miningPhase = 0;


    // 车轮升级：每 30 帧尝试一次，下发后用 ins_ret 判定是否成功（研究已开始），
    // 成功则永久停止；失败或未收到回执则在下一个 30 帧窗口继续尝试
    {
        static int wheelOrderId = -1;
        static int wheelOrderFrame = -10000;

        // 上一条指令结果（ins_ret 仅下一帧有效），必须在节流判断前读取，读到即复位
        if(!wheelDone && wheelOrderId != -1) {
            auto it = info.ins_ret.find(wheelOrderId);
            if(it != info.ins_ret.end()) {
                DBG_OFF("Wheel upgrade ins_ret code = " + to_string(it->second)
                          + " frame = " + to_string(info.GameFrame));
                if(it->second == ACTION_SUCCESS) wheelDone = true;
                wheelOrderId = -1;
            }
        }

        if(!wheelDone && info.GameFrame - wheelOrderFrame >= 30) {
            wheelOrderFrame = info.GameFrame;   // 无论本次是否发出指令，都推进窗口，严格 30 帧一查
            for(auto& building : info.buildings) {
                if(building.Type == BUILDING_MARKET && building.Project == ACT_NULL
                    && info.Meat >= BUILDING_MARKET_WHEEL_UPGRADE_FOOD
                    && info.Wood >= BUILDING_MARKET_WHEEL_UPGRADE_WOOD) {
                    wheelOrderId = BuildingAction(building.SN, BUILDING_MARKET_WHEEL_UPGRADE);
                    break;
                }
            }
        }
    }

    // 市镇中心升级时代（复用 gamePhase1 的 updateStage：100帧节流 + Project==ACT_NULL + ins_ret 观测）
    updateStage();

    // 复合弓科技升级（靶场）：每 30 帧尝试一次，下发后用 ins_ret 判定是否成功（研究已开始），
    // 成功则永久停止；失败或未收到回执则在下一个 30 帧窗口继续尝试
    {
        static int bowOrderId = -1;
        static int bowOrderFrame = -10000;

        if(!compositeBowDone && bowOrderId != -1) {
            auto it = info.ins_ret.find(bowOrderId);
            if(it != info.ins_ret.end()) {
                DBG_OFF("CompositeBow upgrade ins_ret code = " + to_string(it->second)
                          + " frame = " + to_string(info.GameFrame));
                if(it->second == ACTION_SUCCESS) compositeBowDone = true;
                bowOrderId = -1;
            }
        }

        if(!compositeBowDone && info.GameFrame - bowOrderFrame >= 30) {
            bowOrderFrame = info.GameFrame;
            for(auto& building : info.buildings) {
                if(building.Type == BUILDING_RANGE && building.Project == ACT_NULL
                    && info.Meat >= BUILDING_RANGE_UPGRADE_COMPOSITE_BOW_FOOD
                    && info.Wood >= BUILDING_RANGE_UPGRADE_COMPOSITE_BOW_WOOD) {
                    bowOrderId = BuildingAction(building.SN, BUILDING_RANGE_UPGRADE_COMPOSITE_BOW);
                    break;
                }
            }
        }
    }

    // 保留的浆果/羚羊农民持续采集同类资源（浆果列表、羚羊尸体），直至全部枯竭才交回伐木
    if(resourceSwitchDone) {
        berryCollecting();                 // 只维护保留的浆果农民（采集所有开局可见浆果直至枯竭）
        if(huntingPhase == 0) hunting(); else collecting();
        bool DeadGazelle = false;
        for(auto& r : info.resources)
            if(r.Type == RESOURCE_GAZELLE && r.Blood <= 0) { DeadGazelle = true; break; }
        // 羚羊/尸体 全部枯竭后释放猎人，交回伐木；浆果农民由 berryCollecting 自行逐步释放
        // （同步清掉 collecting() 塞入 currentFarmersSN 的标记，防止卡在耕作组无人认领）
        if(!DeadGazelle) {
            // 羚羊尸体收集完毕：全部猎人转伐木组（不进采金组），在伐木组中等待
            // farming() 抽调建田（farming 抽人顺序：采金组优先，伐木组回退）；
            // 显式入组可避免被新生村民交替分配（偶伐木/奇采金）或转采金分支抢走
            for(int sn : hunterFarmersSN) {
                currentFarmersSN.erase(sn);
                treeFarmersSN.insert(sn);
            }
            hunterFarmersSN.clear();
            corpseHelperSNs.clear();
        }
    }

    // ==== 统一分配（仅一次）====：farming 组从伐木工中抽取；repair/stone 组全部归入采金组
    unifiedAssign();

    // 新生成的村民交替分配：第偶数个去砍树，第奇数个去采金（未被任何工种标记且非 builder 的在册村民）
    // 采金组后期封顶 8 人，已满 8 人时新村民一律去砍树
    const int MAX_GOLD_MINERS = 8;
    static int newFarmerIdx = 0;
    for(auto& farmer : info.farmers) {
        if(farmer.SN == builderSN) continue;
        // 开局待命/配对中村民不参与（正常情况下此时配对早已完成，防御性跳过）
        if(farmer.SN == waitingHunterSN || farmer.SN == pairingFarmerSN) continue;
        if(berryFarmersSN.count(farmer.SN) || hunterFarmersSN.count(farmer.SN)) continue;
        if(stoneFarmersSN.count(farmer.SN) || repairFarmersSN.count(farmer.SN)
           || goldFarmersSN.count(farmer.SN)) continue;
        if(treeFarmersSN.count(farmer.SN) || currentFarmersSN.count(farmer.SN)) continue;  // 伐木/耕作组不得吸走
        if(newFarmerIdx % 2 == 0 || (int)goldFarmersSN.size() >= MAX_GOLD_MINERS) {
            treeFarmersSN.insert(farmer.SN);   // 偶数序或采金已满 → 伐木，logging() 会统一下发指令
            DBG_OFF("[assignDBG] new farmer " + to_string(farmer.SN) + " -> TREE #" + to_string(newFarmerIdx)
                      + " gold=" + to_string(goldFarmersSN.size()));
        } else {
            goldFarmersSN.insert(farmer.SN);   // 奇数序且采金未满 → 挖金，goldMining() 会统一下发指令
            DBG_OFF("[assignDBG] new farmer " + to_string(farmer.SN) + " -> GOLD #" + to_string(newFarmerIdx));
        }
        newFarmerIdx++;
    }

    // 维修组动态借调：先保证箭塔安全
    // 1) 敌人在视野范围内（enemy_armies 非空）：立即召回所有借调去挖金的维修工
    //    （从 goldFarmersSN 删除），并对血量最低的受损箭塔直接下令修复；
    // 2) 无敌人且无可修复箭塔（或石料不足）：空闲维修工临时借调去挖金
    if(!repairFarmersSN.empty()) {
        bool enemyInVision = !info.enemy_armies.empty();
        // ==== 调试：repairFarmer 数据流追踪（每 50 帧输出一次，避免刷屏） ====
        static int dbgRepairFrame = -10000;
        if(info.GameFrame - dbgRepairFrame >= 50) {
            dbgRepairFrame = info.GameFrame;
            int borrowed = 0;
            for(int sn : repairFarmersSN)
                if(goldFarmersSN.count(sn)) borrowed++;
            int damagedTowers = 0, minTowerBlood = INT_MAX;
            for(auto& b : info.buildings)
                if(b.Type == BUILDING_ARROWTOWER && b.Blood < b.MaxBlood) {
                    damagedTowers++;
                    minTowerBlood = min(minTowerBlood, b.Blood);
                }
            DBG_OFF("[repairDBG] frame=" + to_string(info.GameFrame)
                      + " repairSN=" + to_string(repairFarmersSN.size())
                      + " borrowedGold=" + to_string(borrowed)
                      + " enemy=" + to_string(info.enemy_armies.size())
                      + " stone=" + to_string(info.Stone)
                      + " damagedTower=" + to_string(damagedTowers)
                      + " minBlood=" + (minTowerBlood == INT_MAX ? string("none") : to_string(minTowerBlood)));
            // 逐人状态（SN/是否在采金组/当前状态），确认指令是否真的下达到人
            for(auto& farmer : info.farmers) {
                if(repairFarmersSN.count(farmer.SN) == 0) continue;
                DBG_OFF("[repairDBG] SN=" + to_string(farmer.SN)
                          + " inGold=" + to_string(goldFarmersSN.count(farmer.SN) ? 1 : 0)
                          + " state=" + to_string(farmer.NowState));
            }
        }
        if(enemyInVision) {
            // 找血量最低的受损箭塔，召回后直接下令修复
            int targetSN = -1, minBlood = INT_MAX;
            for(auto& b : info.buildings) {
                if(b.Type == BUILDING_ARROWTOWER && b.Blood < b.MaxBlood && b.Blood < minBlood) {
                    minBlood = b.Blood;
                    targetSN = b.SN;
                }
            }
            for(auto& farmer : info.farmers) {
                if(repairFarmersSN.count(farmer.SN) == 0) continue;
                if(goldFarmersSN.count(farmer.SN) == 0) continue;   // 未借调，无需召回
                goldFarmersSN.erase(farmer.SN);                     // 从采金组召回
                if(targetSN != -1 && info.Stone >= 50)
                    HumanAction(farmer.SN, targetSN);               // 下令修复
            }
        } else {
            bool towerDamaged = false;
            for(auto& b : info.buildings) {
                if(b.Type == BUILDING_ARROWTOWER && b.Blood < b.MaxBlood) { towerDamaged = true; break; }
            }
            if(!towerDamaged || info.Stone < 50) {
                // 无可修目标：空闲维修工借调去挖金（goldMining() 会为其分配金矿）
                for(auto& farmer : info.farmers) {
                    if(repairFarmersSN.count(farmer.SN) && farmer.NowState == HUMAN_STATE_IDLE)
                        goldFarmersSN.insert(farmer.SN);
                }
            }
        }
    }

    // 统计已建房屋数（含未满血的在建房屋，与 buildTasks 待建去重配合）
    int homeCount = 0;
    for(auto& b : info.buildings)
        if(b.Type == BUILDING_HOME) homeCount++;

    // 金矿旁仓库判定：已建成（满血）且位于金矿簇中心附近（曼哈顿距离 <= 8）
    // 用途：1) 房屋补建至12栋的解锁条件  2) farming 启动判定（下方原逻辑）
    bool goldStockBuilt = false;
    if(goldClusterReady) {
        for(auto& b : info.buildings) {
            if(b.Type != BUILDING_STOCK || b.Blood != b.MaxBlood) continue;
            if(manhattan_distance(b.BlockDR, b.BlockUR, bestCluster.centerX, bestCluster.centerY) <= 8) {
                goldStockBuilt = true; break;
            }
        }
    }

    // 金矿旁仓库建成后：补建房屋至 12 栋
    // 1) 房屋未满 12 前，建筑工持续挂在伐木组备料（logging() 派树砍木；
    //    木材攒够且建筑工 IDLE 时 assignBuilding 的 HumanBuild 直接接管，内核自动挂起伐木关系）；
    //    满 12 后移出伐木组，不再白砍
    // 2) 任务队列全空 且 房屋 < 12 时，排入一个房屋任务（一次一个，建完队列清空后再排下一个；
    //    队列中若有农田等其它任务则让其先行，遵循静态队列顺序、不抢占）
    if(goldStockBuilt) {
        // 防御：建筑工永不入耕作组（历史版本曾被 farming() 抽走连放 8 个农田占位符）
        currentFarmersSN.erase(builderSN);
        if(homeCount < 13) {
            if(builderSN != -1 && treeFarmersSN.count(builderSN) == 0) {
                treeFarmersSN.insert(builderSN);
                DBG_OFF("[homeDBG] builder chopping for home materials, homes=" + to_string(homeCount));
            }
            if(buildTasks.empty()) {
                buildTasks.push_back({BUILDING_HOME, homeX, homeY, {}});
                DBG_OFF("[homeDBG] gold-stock home pushed (queue empty), homes=" + to_string(homeCount)
                          + " wood=" + to_string(info.Wood) + " frame=" + to_string(info.GameFrame));
            }
        } else {
            treeFarmersSN.erase(builderSN);
        }
    }

    // 房屋上限：金矿旁仓库建成前 10 栋，建成后放宽到 13 栋（支撑人口>=48 进第三阶段）
    const int homeCap = goldStockBuilt ? 13 : 10;
    // 调试：人口/上限/已建房屋数 + 未建满时推入任务
    static int dbgHomeFrame = -10000;
    if(info.GameFrame - dbgHomeFrame >= 50) {
        dbgHomeFrame = info.GameFrame;
        DBG_OFF("[homeDBG] frame=" + to_string(info.GameFrame)
                  + " human=" + to_string(info.Human_Num) + "/" + to_string(info.Human_MaxNum)
                  + " homes=" + to_string(homeCount)
                  + " pending=" + to_string(buildTasks.size())
                  + (homeCount >= homeCap ? " CAP_REACHED" : ""));
    }
    if(info.Human_Num >= info.Human_MaxNum - 2 && homeCount < homeCap) {
        // 去重：队列中已有待建房屋（含正在建造的）时不再推入，防止每帧重复 push
        bool homePending = false;
        for(auto& task : buildTasks)
            if(task.Type == BUILDING_HOME) { homePending = true; break; }
        if(!homePending) {
            buildTasks.push_back({BUILDING_HOME, homeX, homeY, {}});
            DBG_OFF("[homeDBG] phase2 home pushed @(" + to_string(homeX) + "," + to_string(homeY)
                      + ") homes=" + to_string(homeCount + 1)
                      + " human=" + to_string(info.Human_Num) + "/" + to_string(info.Human_MaxNum)
                      + " frame=" + to_string(info.GameFrame));
        }
    }

    // farming 启动条件（与"伐木工转采金"解耦）：
    //   学院已建成 + 金矿旁仓库已建成 + 车轮科技已研究完毕
    // 学院已建成判定：BUILDING_COLLAGE 存在且 Blood == MaxBlood；
    // 金矿旁仓库已建成判定：BUILDING_STOCK 存在且 Blood == MaxBlood，
    //   且其位置在金矿簇中心 (bestCluster.centerX, bestCluster.centerY) 附近（曼哈顿距离 <= 8）；
    //   依赖 goldMining() 首帧在 bestCluster/goldClusterReady 里写入簇中心坐标。
    // 车轮科技判定：wheelDone（函数级 static，车轮升级块在 ins_ret == ACTION_SUCCESS 后置 true）
    {
        bool collageBuilt = false;
        for(auto& b : info.buildings)
            if(b.Type == BUILDING_COLLAGE && b.Blood == b.MaxBlood) { collageBuilt = true; break; }
        if(collageBuilt && goldStockBuilt && wheelDone) {
            // 建造农场前专职抽调（2026-10-08）：抽2名村民——1人采石（供箭塔维修
            // 石料，stoneMining() 每帧调度）、1人常设修箭塔（fixingArrowTower()
            // 每帧调度，无塔可修/石料不足时临时借调挖金，敌袭受损即召回抢修）。
            // 一次性抽足2人后置位不再重复；先抽采金组、无人回退伐木组（同
            // farming() 抽人惯例），跳过 builder 与已在维修/采石组的人员。
            static bool farmStaffPulled = false;
            if(!farmStaffPulled) {
                auto pullStaff = [&]() -> int {
                    for(int sn : goldFarmersSN) {
                        if(sn == builderSN || repairFarmersSN.count(sn) || stoneFarmersSN.count(sn)) continue;
                        goldFarmersSN.erase(sn);
                        return sn;
                    }
                    for(int sn : treeFarmersSN) {
                        if(sn == builderSN || repairFarmersSN.count(sn) || stoneFarmersSN.count(sn)) continue;
                        treeFarmersSN.erase(sn);
                        return sn;
                    }
                    return -1;
                };
                int stoneSN = pullStaff();
                int repairSN = pullStaff();
                if(stoneSN != -1) stoneFarmersSN.insert(stoneSN);
                if(repairSN != -1) repairFarmersSN.insert(repairSN);
                if(stoneSN != -1 && repairSN != -1) {
                    farmStaffPulled = true;
                    DebugText("[farmDBG] 农场期专职抽调：采石村民=" + to_string(stoneSN)
                              + " 修塔村民=" + to_string(repairSN)
                              + " frame=" + to_string(info.GameFrame));
                }
            }
            // farming 启动。但不在这里转组——"伐木工后期全部转采金"由下方的
            // 独立分支（woodNeed 计算）控制，两逻辑互不依赖。
            farming();
        }
    }

    // 伐木工后期转采金分支：当前木头足够用到游戏结束——剩余房屋（上限 homeCap，含队列中待建）+
    // 剩余农田（上限8块）+ 未完成的复合弓/车轮科技的全部木材需求
    // 注意：本分支与 farming 启动解耦，触发时车轮科技未必已完成，仍需预留其木材
    {
        int pendingHomes = 0;
        for(auto& task : buildTasks)
            if(task.Type == BUILDING_HOME) pendingHomes++;
        const int FARM_SLOTS = 8;   // farmDx/farmDy 数组长度，与 farming() 保持一致
        int woodNeed = max(0, homeCap - homeCount - pendingHomes) * getBuildingWoodCost(BUILDING_HOME)
                     + max(0, FARM_SLOTS - farmNum) * BUILD_FARM_WOOD
                     + (compositeBowDone ? 0 : BUILDING_RANGE_UPGRADE_COMPOSITE_BOW_WOOD)
                     + (wheelDone        ? 0 : BUILDING_MARKET_WHEEL_UPGRADE_WOOD);
        if(info.Wood >= woodNeed) {
            // 木头已够用到终局：剩余未耕种村民全部转采金矿。
            // 伐木组保留人力仅作 farming() 抽人回退用（farming 优先从采金组抢人，
            // 采金组无人时才回退伐木组）；浆果/猎人/维修/采石组有各自的生命周期
            // 管理，不在此抢人（避免同帧双重 HumanAction 冲突）
            int reserve = max(0, FARM_SLOTS - farmNum);
            for(auto& farmer : info.farmers) {
                if(farmer.SN == builderSN) continue;
                if(farmer.SN == waitingHunterSN || farmer.SN == pairingFarmerSN) continue;
                if(currentFarmersSN.count(farmer.SN)) continue;              // 已在耕作
                bool goldFull = (int)goldFarmersSN.size() >= MAX_GOLD_MINERS; // 采金封顶8人，多余留伐木
                if(treeFarmersSN.count(farmer.SN)) {
                    if(reserve > 0) { reserve--; continue; }                 // 留作建田人力
                    if(goldFull) continue;                                   // 采金已满：继续砍树
                    treeFarmersSN.erase(farmer.SN);                          // 移出伐木组
                    goldFarmersSN.insert(farmer.SN);                         // 转采金
                } else if(!berryFarmersSN.count(farmer.SN) && !hunterFarmersSN.count(farmer.SN)
                          && !repairFarmersSN.count(farmer.SN) && !stoneFarmersSN.count(farmer.SN)
                          && !goldFarmersSN.count(farmer.SN)) {
                    if(goldFull) treeFarmersSN.insert(farmer.SN);            // 无组空闲村民：满了去砍树
                    else         goldFarmersSN.insert(farmer.SN);            // 未满则直接采金
                }
            }
        }
    }
    // 采金组封顶：第二阶段后期最多 8 人采金，多余的（非维修借调）退回伐木组。
    // 处理一次性合组（stone→gold）等历史超员；维修借调人员优先保留（敌袭时需召回修塔）。
    while((int)goldFarmersSN.size() > MAX_GOLD_MINERS) {
        int movedSN = -1;
        for(int sn : goldFarmersSN) {
            if(repairFarmersSN.count(sn) == 0) { movedSN = sn; break; }   // 优先移除专职采金
        }
        if(movedSN == -1) break;   // 剩下的全是维修借调，保留
        goldFarmersSN.erase(movedSN);
        treeFarmersSN.insert(movedSN);
        DBG_OFF("[assignDBG] gold cap 8 reached, farmer " + to_string(movedSN) + " -> TREE");
    }
    goldMining();     // 采金组（含原 repair/stone 组 + 新村民）
    createArmy1();
    logging();
}

bool UsrAI::checkArmy1() {
    // 第二阶段毕业条件（2026-10-07 调整）：当前人口数 >= 48 即进入第三阶段反攻
    // （房屋上限13支撑人口；方阵兵10/复合弓兵18生产上限不变，仅更换阶段触发条件）
    return info.Human_Num < 48;
}

void UsrAI::gamePhase3() { // 全面反攻
    static int lastOrderFrame = -1;
    static map<int, pair<int,int>> lastBlock;   // 每支部队上一拍块坐标（卡脚检测）
    static map<int, int> stuckSince;            // 块坐标未变化的起始帧
    static map<int, pair<int,int>> wdLastPos;   // 看门狗：每支部队上一拍块坐标
    static map<int, int> wdSameBeats;           // 看门狗：连续原地未动拍数
    static int phase3StartFrame = -1;           // 第三阶段开始帧（集结超时判定）
    if(info.GameFrame - lastOrderFrame < 30) return;
    lastOrderFrame = info.GameFrame;
    if(phase3StartFrame == -1) phase3StartFrame = info.GameFrame;
    phase3Active = true;

    // 反攻期间持续补员至 方阵兵10 / 复合弓兵18（createArmy1 按实际存活数判断）
    createArmy1();

    // ===== 集结点（2026-10-07 重做）：双兵种分点集结 =====
    // 方阵兵：最靠近地图中心(50,50)的合法 5x5 空地中心；
    // 复合弓兵：市镇中心-方阵兵集结区中心连线中点附近寻找合法 5x5 空地。
    // 合法性只算静态障碍（地形/资源/双方建筑），忽略人类单位——集结时站位
    // 本就会挤在一起，把单位算作障碍会导致永远找不到空地。找到后缓存。
    static int rallyBlock[MAP_SIZE][MAP_SIZE];
    static bool rallyBlockInit = false;
    if(!rallyBlockInit) {
        rallyBlockInit = true;
        for(int i = 0; i < MAP_SIZE; i++)
            for(int j = 0; j < MAP_SIZE; j++)
                rallyBlock[i][j] = 0;
        // 地形障碍：迷雾/海洋/浅滩/高度-1(悬崖)
        if(info.theMap != nullptr) {
            for(int i = 0; i < MAP_SIZE; i++)
                for(int j = 0; j < MAP_SIZE; j++) {
                    const tagTerrain& tt = (*info.theMap)[i][j];
                    if(tt.type == MAPPATTERN_UNKNOWN || tt.type == MAPPATTERN_OCEAN
                       || tt.type == MAPPATTERN_SHOAL || tt.height == -1)
                        rallyBlock[i][j] = 1;
                }
        }
        // 静态资源障碍（动物可移动/会死，不计入）
        for(auto& r : info.resources) {
            if(r.Type != RESOURCE_TREE && r.Type != RESOURCE_STONE
               && r.Type != RESOURCE_GOLD && r.Type != RESOURCE_BUSH) continue;
            int s = getResourceSideLen(r.Type);
            for(int dx = 0; dx < s; dx++)
                for(int dy = 0; dy < s; dy++) {
                    int nx = r.BlockDR + dx, ny = r.BlockUR + dy;
                    if(nx >= 0 && nx < MAP_SIZE && ny >= 0 && ny < MAP_SIZE)
                        rallyBlock[nx][ny] = 1;
                }
        }
        // 双方建筑占地
        auto markRallyB = [&](const vector<tagBuilding>& bs) {
            for(auto& b : bs) {
                int s = getBuildingSideLen(b.Type);
                for(int dx = 0; dx < s; dx++)
                    for(int dy = 0; dy < s; dy++) {
                        int nx = b.BlockDR + dx, ny = b.BlockUR + dy;
                        if(nx >= 0 && nx < MAP_SIZE && ny >= 0 && ny < MAP_SIZE)
                            rallyBlock[nx][ny] = 1;
                    }
            }
        };
        markRallyB(info.buildings);
        markRallyB(info.enemy_buildings);
    }

    // 在 (ax,ay) 附近按曼哈顿距离向外扩环搜索 5x5 全空区域中心
    auto find5x5 = [&](int ax, int ay) -> pair<int,int> {
        const int RS = 5, HALF = 2;
        for(int r = 0; r < MAP_SIZE; r++) {
            for(int dx = -r; dx <= r; dx++) {
                int dyabs = r - abs(dx);
                for(int yi = 0; yi < 2; yi++) {
                    if(yi == 1 && dyabs == 0) break;        // r=0 去重
                    int cx = ax + dx, cy = ay + (yi == 0 ? dyabs : -dyabs);
                    if(cx < HALF || cx > MAP_SIZE - 1 - HALF
                       || cy < HALF || cy > MAP_SIZE - 1 - HALF) continue;
                    int x0 = cx - HALF, y0 = cy - HALF;
                    bool ok = true;
                    for(int i = 0; i < RS && ok; i++)
                        for(int j = 0; j < RS; j++)
                            if(rallyBlock[x0 + i][y0 + j]) { ok = false; break; }
                    if(ok) return {cx, cy};
                }
            }
        }
        return {ax, ay};   // 整图无 5x5 空地时兜底锚点本身
    };

    static int hopRallyX = -1, hopRallyY = -1;   // 方阵兵(+祭司)集结区中心
    static int bowRallyX = -1, bowRallyY = -1;   // 复合弓兵集结区中心
    // 2026-10-08 对调：复合弓兵 → 最靠近地图中心(50,50)的合法 5x5 空地；
    // 方阵兵+祭司 → 市镇中心-该点连线中点附近的合法 5x5 空地
    if(bowRallyX == -1) {
        auto p = find5x5(50, 50);
        bowRallyX = p.first; bowRallyY = p.second;
        DebugText("[phase3] 复合弓兵集结点(5x5中心)=(" + to_string(bowRallyX) + ","
                  + to_string(bowRallyY) + ")");
    }
    if(hopRallyX == -1) {
        int midX = (centerX + bowRallyX) / 2, midY = (centerY + bowRallyY) / 2;
        auto p = find5x5(midX, midY);
        hopRallyX = p.first; hopRallyY = p.second;
        DebugText("[phase3] 方阵兵+祭司集结点(5x5中心)=(" + to_string(hopRallyX) + ","
                  + to_string(hopRallyY) + ") 连线中点锚=(" + to_string(midX) + ","
                  + to_string(midY) + ")");
    }

    // 到达判定：各兵种进入自己 5x5 区域（中心切比雪夫<=2）的人数达到该兵种
    // 总数 60% 即集结完成，向敌军大本营前进；其它兵种（战车/投石车等）随方阵兵
    // 区域集结。祭司不参与统计（由 priest() 单独调度）。超时1000帧强制开进。
    static bool allGathered = false;
    int hopTotal = 0, hopIn = 0, bowTotal = 0, bowIn = 0, othTotal = 0, othIn = 0;
    for(auto& army : info.armies) {
        if(army.Sort == AT_PRIEST) continue;
        bool inHop = max(abs(army.BlockDR - hopRallyX), abs(army.BlockUR - hopRallyY)) <= 2;
        bool inBow = max(abs(army.BlockDR - bowRallyX), abs(army.BlockUR - bowRallyY)) <= 2;
        if(army.Sort == AT_COMPOSITE_BOWMAN) { bowTotal++; if(inBow) bowIn++; }
        else if(army.Sort == AT_HOPLITE)     { hopTotal++; if(inHop) hopIn++; }
        else                                 { othTotal++; if(inHop) othIn++; }
    }
    auto reach60 = [](int in, int total) { return total == 0 || in * 5 >= total * 3; };
    if(!allGathered) {
        if(reach60(hopIn, hopTotal) && reach60(bowIn, bowTotal) && reach60(othIn, othTotal)) {
            allGathered = true;
            DebugText("[phase3] 双点集结完成，向敌军大本营前进 frame=" + to_string(info.GameFrame)
                      + " hop=" + to_string(hopIn) + "/" + to_string(hopTotal)
                      + " bow=" + to_string(bowIn) + "/" + to_string(bowTotal)
                      + " other=" + to_string(othIn) + "/" + to_string(othTotal));
        } else if(info.GameFrame - phase3StartFrame > 1000) {
            allGathered = true;
            DebugText("[phase3] 集结超时1000帧，强制开始反攻 frame=" + to_string(info.GameFrame)
                      + " hop=" + to_string(hopIn) + "/" + to_string(hopTotal)
                      + " bow=" + to_string(bowIn) + "/" + to_string(bowTotal)
                      + " other=" + to_string(othIn) + "/" + to_string(othTotal));
        }
    }

    // 每5s(125帧)诊断：未开进原因 + 所有兵种类/位置 + 集结点位置
    static int lastRallyDbgFrame = -10000;
    if(!allGathered && info.GameFrame - lastRallyDbgFrame >= 125) {
        lastRallyDbgFrame = info.GameFrame;
        string reason;
        if(!reach60(hopIn, hopTotal))
            reason += "方阵兵未到齐(" + to_string(hopIn) + "/" + to_string(hopTotal) + ") ";
        if(!reach60(bowIn, bowTotal))
            reason += "复合弓兵未到齐(" + to_string(bowIn) + "/" + to_string(bowTotal) + ") ";
        if(!reach60(othIn, othTotal))
            reason += "其它兵种未到齐(" + to_string(othIn) + "/" + to_string(othTotal) + ") ";
        string posList;
        for(auto& a : info.armies) {
            string nm = a.Sort == AT_HOPLITE ? "方阵兵"
                      : a.Sort == AT_COMPOSITE_BOWMAN ? "复合弓兵"
                      : a.Sort == AT_STONE_THROWER ? "投石车"
                      : a.Sort == AT_PRIEST ? "祭司"
                      : "兵种" + to_string(a.Sort);
            posList += nm + "#" + to_string(a.SN) + "@(" + to_string(a.BlockDR)
                     + "," + to_string(a.BlockUR) + ") ";
        }
        DebugText("[phase3集结] 未开进原因:" + reason
                  + "| 集结点 方阵(" + to_string(hopRallyX) + "," + to_string(hopRallyY)
                  + ") 复合弓(" + to_string(bowRallyX) + "," + to_string(bowRallyY) + ")"
                  + " | " + posList);
    }

    //敌方中心：优先用已侦察到的敌方攻城武器厂(与敌方AI守军的Enemy_Center同源)，
    //未侦察到时用我方中心的镜像推断
    int ecx = 100 - centerX, ecy = 100 - centerY;
    for(auto& b : info.enemy_buildings) {
        if(b.Type == BUILDING_SIEGE && b.Blood > 0) { ecx = b.BlockDR; ecy = b.BlockUR; break; }
    }

    // ===== 前线哨位体系：分兵种纵深配置 + 缓步推进，严禁整队冲进敌方大本营 =====
    // 敌方厂区守军追出25格(DEFENSE_CHASE_LIMIT)即折返；前排停在距敌中心24格附近
    // 正好接住追出的守军。纵深：方阵兵前排(D-2) → 复合弓兵中排(D+4)后排集火 →
    // 投石车(D+7)/祭司(D+8)殿后，D=前排基准距敌中心格数。
    // 推进规则（取代旧版"无敌军可视时直接把目的地设为敌中心"的整队冲锋——
    // 投石车AOE曾一轮团灭全部复合弓兵）：
    //   敌人未贴近前排哨位(nearThreat=false)且不掉血持续300帧 → 前线向敌中心缓步推进4格；
    //   敌人未贴近但持续掉血（被箭塔/投石车等静态火力点名）→ 后撤4格脱离射程；
    //   推进到下限10格后持续安静600帧 或 后撤满3次 → 发起最终总攻
    //   （方阵兵突入敌营坦克火力，复合弓兵紧随其后集火投石车）。
    static const int EDGE_DIST = 24;
    static const int CREEP_MIN_DIST = 10;
    static const int CREEP_STEP = 4;
    static int s_creepBlocks = 0;
    static int s_quietSince = -1;
    static int s_retreats = 0;
    static long long s_lastBloodSum = -1;
    static bool s_finalPush = false;
    phase3FinalPush = s_finalPush;   // 同步给 priest()：总攻前祭司只在地图中心待命

    // ===== 战斗阶段识别：从第三阶段开始实时统计全军消灭的近战敌人数量 =====
    // 追踪已见敌方近战单位 SN，从视野中消失即计为消灭；消灭数>25 → 击杀达标
    // （战斗阶段指标，解锁弓兵进军/塔攻坚/交战边界放开；方阵兵冲锋只看 s_finalPush）
    static set<int> seenMeleeSNs;
    static int meleeKills = 0;
    {
        set<int> nowMelee;
        for(auto& e : info.enemy_armies) {
            if(e.Blood > 0 && getAttackRange(e.Sort) <= 1) nowMelee.insert(e.SN);
        }
        for(auto it = seenMeleeSNs.begin(); it != seenMeleeSNs.end();) {
            if(nowMelee.count(*it) == 0) { meleeKills++; it = seenMeleeSNs.erase(it); }
            else ++it;
        }
        seenMeleeSNs.insert(nowMelee.begin(), nowMelee.end());
    }
    static bool s_chargeTriggered = false;
    // 阈值25（2026-10-08 调整，旧值8过早冲锋）：全军击杀25个近战敌人后击杀达标
    if(!s_chargeTriggered && meleeKills > 25) {
        s_chargeTriggered = true;
        DebugText("[phase3] 近战敌人消灭" + to_string(meleeKills)
                  + "个>25，击杀达标（方阵兵仍驻守哨点，冲锋待最终总攻） frame="
                  + to_string(info.GameFrame));
    }

    // ===== 敌方箭塔摧毁计数（2026-10-08规则）：最终反攻阶段累计完全摧毁3座箭塔前，
    // 祭祀不得脱离哨点行动。摧毁判定：已见箭塔SN从存活列表消失（打空血即移除）。
    // 最终攻夺同样须等解锁后才触发——否则触发后全军弃攻贴塔吸火力，再无人拆塔，
    // 3座摧毁数永远凑不齐，祭司被永久锁死在哨点（死锁）。
    static set<int> seenTowerSNs;
    static int towersDestroyed = 0;
    {
        set<int> nowTowers;
        for(auto& b : info.enemy_buildings)
            if(b.Type == BUILDING_ARROWTOWER && b.Blood > 0) nowTowers.insert(b.SN);
        for(auto it = seenTowerSNs.begin(); it != seenTowerSNs.end();) {
            if(nowTowers.count(*it) == 0) { towersDestroyed++; it = seenTowerSNs.erase(it); }
            else ++it;
        }
        seenTowerSNs.insert(nowTowers.begin(), nowTowers.end());
    }
    if(!priestTowerUnlocked && towersDestroyed >= 3) {
        priestTowerUnlocked = true;
        DebugText("[phase3] 敌方箭塔已完全摧毁" + to_string(towersDestroyed)
                  + "座(≥3)，祭祀解除哨点限制 frame=" + to_string(info.GameFrame));
    }

    // ===== 复合弓兵战场态势判断 → 主动进军 =====
    // 战斗后期定义：双点集结完成 且（方阵兵冲锋已触发 或 已进入最终总攻）；
    // 警戒范围：任一复合弓兵10格（切比雪夫）内出现敌方近战单位即视为受威胁；
    // 战斗后期持续15秒(375帧)无敌方近战进逼 → 主动进军敌方大本营（一次触发）。
    // 进军途中保留即时反应能力：交战段的贴脸风筝/集火逻辑优先于移动指令；
    // 阵型由内核寻路自动保持最小作战间距（同目标多点排队站位）。
    static int bowCalmSince = -1;      // 警戒范围无敌方近战的起始帧
    static bool bowAdvance = false;    // 主动进军（向敌方大本营）
    {
        bool meleeNear = false;
        for(auto& army : info.armies) {
            if(army.Sort != AT_COMPOSITE_BOWMAN || army.Blood <= 0) continue;
            for(auto& e : info.enemy_armies) {
                if(e.Blood <= 0 || getAttackRange(e.Sort) > 1) continue;
                if(max(abs(e.BlockDR - army.BlockDR), abs(e.BlockUR - army.BlockUR)) <= 10) {
                    meleeNear = true; break;
                }
            }
            if(meleeNear) break;
        }
        if(meleeNear) bowCalmSince = -1;
        else if(bowCalmSince == -1) bowCalmSince = info.GameFrame;
        // 敌方基地已侦察到（箭塔/攻城厂进视野）也算战斗后期——防止守军全龟缩、
        // 只挨箭塔远程打而近战击杀链条无法启动的死锁
        bool enemyBaseVisible = false;
        for(auto& b : info.enemy_buildings)
            if((b.Type == BUILDING_ARROWTOWER || b.Type == BUILDING_SIEGE) && b.Blood > 0) {
                enemyBaseVisible = true; break;
            }
        if(!bowAdvance && allGathered && (s_chargeTriggered || s_finalPush || enemyBaseVisible)
           && bowCalmSince != -1 && info.GameFrame - bowCalmSince >= 375) {
            bowAdvance = true;
            DebugText("[phase3] 复合弓兵15秒无敌方近战进逼，主动进军敌方大本营 frame="
                      + to_string(info.GameFrame));
        }
    }

    // ===== 祭祀"最终攻夺"触发判定（2026-10-08 调整）：第3座箭塔倒下即触发 =====
    // 门禁：敌方箭塔完全摧毁≥3座(priestTowerUnlocked)。解锁当拍直接发起最终攻夺，
    // 全军立即弃攻贴塔吸火——不再等"下一座塔血<50"（旧判定）。
    // 攻城厂尚未侦察到时不落定目标，持续每拍重试直至其进视野后立即触发。
    if(!finalSiegeCaptureTriggered && priestTowerUnlocked) {
        for(auto& b : info.enemy_buildings) {
            if(b.Type == BUILDING_SIEGE && b.Blood > 0 && b.Percent >= 100) {
                finalSiegeCaptureTriggered = true;
                finalSiegeSN = b.SN;
                finalSiegeX = b.BlockDR;
                finalSiegeY = b.BlockUR;
                finalSiegeTriggerFrame = info.GameFrame;
                DebugText("[phase3] 敌方箭塔已摧毁3座，直接触发祭祀最终攻夺(跳过塔血<50判定，祭司将先停留2秒再动身)：攻城厂 SN="
                          + to_string(finalSiegeSN) + " @(" + to_string(finalSiegeX)
                          + "," + to_string(finalSiegeY) + ") frame=" + to_string(info.GameFrame));
                break;
            }
        }
    }

    double ddx = centerX - ecx, ddy = centerY - ecy;
    double dlen = sqrt(ddx*ddx + ddy*ddy);
    double uEx = 0, uEy = 0;   // 我方中心->敌方中心 单位方向
    if(dlen > 1e-6) { uEx = -ddx / dlen; uEy = -ddy / dlen; }
    bool enemyFound = info.enemy_armies.size() > 0;
    // 交战边界半径（下方攻击分配段共用）：缓步推进期收紧(10格)严禁越界追击；
    // 击杀达标/复合弓兵主动进军/最终总攻 任一触发即放开(40格≈全图)
    const int ENGAGE_DIST = (s_finalPush || s_chargeTriggered || bowAdvance) ? 40 : 10;

    // ===== 诱饵方阵兵（2026-10-08）：后期诱敌改由单个方阵兵执行 =====
    // 旧方案靠投石车压前诱敌（风险高且挤占拆塔输出），现固定指派一名方阵兵
    // 前出至敌守军警戒圈(15格)内侧拉扯，把守军逐股拖出箭塔保护圈交弓兵集火。
    // 诱饵阵亡自动指派下一名方阵兵；无可派方阵兵时诱敌暂停。
    static int s_decoySN = -1;
    {
        bool decoyAlive = false;
        for(auto& a : info.armies) {
            if(a.SN == s_decoySN && a.Sort == AT_HOPLITE && a.Blood > 0) { decoyAlive = true; break; }
        }
        if(!decoyAlive) {
            s_decoySN = -1;
            for(auto& a : info.armies) {
                if(a.Sort == AT_HOPLITE && a.Blood > 0) { s_decoySN = a.SN; break; }
            }
        }
    }

    // 血量总和排除祭司——祭司被远程点射风筝是常态，不应触发全军后撤；
    // 执勤中的诱饵方阵兵同样排除——吸火掉血是本职，不应误触"遭火力消耗后撤"
    long long bloodSum = 0;
    for(auto& a : info.armies) {
        if(a.Sort == AT_PRIEST) continue;
        if(allGathered && !s_finalPush && a.SN == s_decoySN) continue;
        bloodSum += a.Blood;
    }
    // 注：不再因部队数量变化重置 s_quietSince（2026-10-08 修复）——phase3 持续补员，
    // 旧逻辑每次新兵入列就清零安静计时，"推进到底后600帧安静→总攻"永远凑不满。
    // 掉血(bled)仍照常重置安静计时。

    // ---- 哨位几何先算（状态机需要钳制后的前排哨位来判定敌人是否真正贴近）----
    int lineDist = max(EDGE_DIST - s_creepBlocks, CREEP_MIN_DIST);
    auto postAt = [&](int dist) -> pair<int,int> {
        int x = ecx - (int)(uEx * dist + (uEx >= 0 ? 0.5 : -0.5));
        int y = ecy - (int)(uEy * dist + (uEy >= 0 ? 0.5 : -0.5));
        return { max(0, min(MAP_SIZE - 1, x)), max(0, min(MAP_SIZE - 1, y)) };
    };

    // ---- 连通域钳制：直线哨位可能落在海洋/浅滩/悬崖或被建筑围死的孤格上——内核对
    //      不可达目的地会静默拒绝 HumanMove（单位 IDLE 永驻原地），上一局"集合后
    //      全军冻结到结束"即此根因。以存活作战部队所在格 BFS 泛洪出实际可达陆地
    //      区域，所有哨位强制钳制到该区域内。----
    static int blockGrid[MAP_SIZE][MAP_SIZE];
    static int compBlock[MAP_SIZE][MAP_SIZE];
    for(int i = 0; i < MAP_SIZE; i++)
        for(int j = 0; j < MAP_SIZE; j++) { blockGrid[i][j] = 0; compBlock[i][j] = 0; }
    if(info.theMap != nullptr) {
        // 迷雾(MAPPATTERN_UNKNOWN)不算障碍（2026-10-08 修复）：它是未知而非不可通行，
        // 旧逻辑把迷雾当障碍导致 BFS 可达域止步于已探索边缘、所有哨位被钳回我方
        // 已探索区——全军永远"萎缩在敌阵线外"。海洋/浅滩/悬崖仍为真实障碍；
        // 若迷雾下实际是海洋，内核拒绝移动，由看门狗日志暴露。
        for(int i = 0; i < MAP_SIZE; i++)
            for(int j = 0; j < MAP_SIZE; j++) {
                const tagTerrain& tt = (*info.theMap)[i][j];
                if(tt.type == MAPPATTERN_OCEAN
                   || tt.type == MAPPATTERN_SHOAL || tt.height == -1)
                    blockGrid[i][j] = 1;
            }
    }
    for(auto& r : info.resources) {
        if(r.Type != RESOURCE_TREE && r.Type != RESOURCE_STONE
           && r.Type != RESOURCE_GOLD && r.Type != RESOURCE_BUSH) continue;
        int s = getResourceSideLen(r.Type);
        for(int dx = 0; dx < s; dx++)
            for(int dy = 0; dy < s; dy++) {
                int nx = r.BlockDR + dx, ny = r.BlockUR + dy;
                if(nx >= 0 && nx < MAP_SIZE && ny >= 0 && ny < MAP_SIZE)
                    blockGrid[nx][ny] = 1;
            }
    }
    auto markBuilds3 = [&](const vector<tagBuilding>& bs) {
        for(auto& b : bs) {
            int s = getBuildingSideLen(b.Type);
            for(int dx = 0; dx < s; dx++)
                for(int dy = 0; dy < s; dy++) {
                    int nx = b.BlockDR + dx, ny = b.BlockUR + dy;
                    if(nx >= 0 && nx < MAP_SIZE && ny >= 0 && ny < MAP_SIZE)
                        blockGrid[nx][ny] = 1;
                }
        }
    };
    markBuilds3(info.buildings);
    markBuilds3(info.enemy_buildings);
    long long cenSx = 0, cenSy = 0; int cenCnt = 0;
    vector<pair<int,int>> flood;
    for(auto& a : info.armies) {
        if(a.Sort == AT_PRIEST) continue;
        int x = max(0, min(MAP_SIZE - 1, a.BlockDR));
        int y = max(0, min(MAP_SIZE - 1, a.BlockUR));
        cenSx += x; cenSy += y; cenCnt++;
        if(blockGrid[x][y] == 0 && compBlock[x][y] != 2) {
            compBlock[x][y] = 2;
            flood.push_back(make_pair(x, y));
        }
    }
    for(size_t fi = 0; fi < flood.size(); fi++) {
        int cx0 = flood[fi].first, cy0 = flood[fi].second;
        // 8 向泛洪（2026-10-08）：内核单位可斜向通行，4 向 BFS 会漏判仅斜向连通
        // 的陆桥/隘口，导致可达域被人为切断、绕湖路点找不到
        const int dx8[8] = {1, -1, 0, 0, 1, 1, -1, -1};
        const int dy8[8] = {0, 0, 1, -1, 1, -1, 1, -1};
        for(int k = 0; k < 8; k++) {
            int nx = cx0 + dx8[k], ny = cy0 + dy8[k];
            if(nx < 0 || nx >= MAP_SIZE || ny < 0 || ny >= MAP_SIZE) continue;
            if(blockGrid[nx][ny] == 0 && compBlock[nx][ny] != 2) {
                compBlock[nx][ny] = 2;
                flood.push_back(make_pair(nx, ny));
            }
        }
    }
    int cenX = cenCnt > 0 ? (int)(cenSx / cenCnt) : centerX;
    int cenY = cenCnt > 0 ? (int)(cenSy / cenCnt) : centerY;
    // clampPost：哨位在连通域内→原样；域外（湖/海洋/悬崖挡住直线）→全图扫描
    // 距目标最近的可达格作为绕行路点（2026-10-08 修复：旧版沿目标→质心连线
    // 直线回退，遇湖只会退回本侧湖岸，部队永远停在湖边罚站；若陆地绕湖连通，
    // 最近可达格落在对岸，内核寻路自动带队绕湖前进）
    auto clampPost = [&](pair<int,int> p) -> pair<int,int> {
        int x = max(0, min(MAP_SIZE - 1, p.first));
        int y = max(0, min(MAP_SIZE - 1, p.second));
        if(compBlock[x][y] == 2) return make_pair(x, y);
        pair<int,int> best = make_pair(cenX, cenY);
        int bestD = INT_MAX;
        for(int i = 0; i < MAP_SIZE; i++) {
            for(int j = 0; j < MAP_SIZE; j++) {
                if(compBlock[i][j] != 2) continue;
                int dx = i - x, dy = j - y;
                int d = dx * dx + dy * dy;
                if(d < bestD) { bestD = d; best = make_pair(i, j); }
            }
        }
        return best;
    };
    pair<int,int> clampedFront = clampPost(s_finalPush ? make_pair(ecx, ecy) : postAt(lineDist - 2));

    // ---- 状态机：与"远处可见敌人"解耦。旧版把任何可见敌军(enemyFound)都当威胁：
    //      远处守军一露头 quiet 永不累计，后撤/推进/总攻全部瘫痪（上一局第二根因）。
    //      新逻辑只看"敌人是否贴近前排哨位(nearThreat)"：贴近→交战期，quiet 暂停，
    //      由攻击分配段接管；远处露头但贴不上来→照常安静/推进。
    bool nearThreat = false;
    for(auto& e : info.enemy_armies) {
        if(e.Blood <= 0) continue;
        if(max(abs(e.BlockDR - clampedFront.first), abs(e.BlockUR - clampedFront.second)) <= ENGAGE_DIST) {
            nearThreat = true;
            break;
        }
    }
    bool bled = (s_lastBloodSum != -1 && bloodSum < s_lastBloodSum);
    if(nearThreat) {
        s_quietSince = -1;   // 敌人已贴近前排：交战中，不推进不后撤
    } else if(!s_finalPush) {
        if(bled) {
            // 敌人未贴近但持续掉血：被箭塔/投石车等静态火力点名，后撤脱离射程
            s_quietSince = -1;
            if(s_creepBlocks > 0) {
                s_creepBlocks = max(0, s_creepBlocks - CREEP_STEP);
                s_retreats++;
                DebugText("[phase3] 前线遭建筑火力消耗后撤，距敌中心 "
                          + to_string(max(EDGE_DIST - s_creepBlocks, CREEP_MIN_DIST)) + " 格 frame="
                          + to_string(info.GameFrame));
                if(s_retreats >= 3) {
                    s_finalPush = true;
                    DebugText("[phase3] 后撤满3次，发起最终总攻 frame=" + to_string(info.GameFrame));
                }
            }
        } else if(s_quietSince == -1) {
            s_quietSince = info.GameFrame;
        } else if(info.GameFrame - s_quietSince > 300) {
            if(s_creepBlocks < EDGE_DIST - CREEP_MIN_DIST) {
                s_creepBlocks += CREEP_STEP;
                DebugText("[phase3] 前线缓步推进至距敌中心 "
                          + to_string(max(EDGE_DIST - s_creepBlocks, CREEP_MIN_DIST)) + " 格 frame="
                          + to_string(info.GameFrame));
                s_quietSince = info.GameFrame;   // 只有真正推进了才重置计时
            } else if(info.GameFrame - s_quietSince > 600) {
                s_finalPush = true;
                DebugText("[phase3] 前线安静600帧，发起最终总攻 frame=" + to_string(info.GameFrame));
            }
            // 蠕变已到底但安静未满600帧：不重置，让计时继续累计到600触发总攻
            // （2026-10-08 修复：旧版此处无条件重置 s_quietSince，elapsed 永远在
            //  300~330 之间徘徊，600 分支永不执行——湖畔停滞期"总攻=否"的直接根因）
        }
    }
    s_lastBloodSum = bloodSum;

    // 四哨位（状态机本拍更新的 s_finalPush/s_creepBlocks 下一拍生效，滞后一拍可接受）
    pair<int,int> frontPost  = clampedFront;                                              // 方阵兵前排
    pair<int,int> bowPost    = clampPost(s_finalPush ? postAt(6) : postAt(lineDist + 4)); // 复合弓兵中排
    // 投石车与弓兵站在一块进攻（2026-10-08）：不单独前压进守军警戒范围诱敌，
    // 就在弓兵阵线上原地拆塔/对射，守军来犯由弓兵集火与方阵兵哨点体系应对
    pair<int,int> stonePost  = clampPost(s_finalPush ? postAt(6) : postAt(lineDist + 4));
    // 祭司锚点（2026-10-08 新规）：最终总攻前只待在地图中心(50,50)待命，不随军
    // 跟进、不参与前线转化；总攻后压至距敌中心15格（塔射程7的两倍余量）；
    // 最终攻夺触发后由 priest() 的攻夺分支自行压进（走到攻城厂12格视野边缘即转化）
    pair<int,int> priestPost = clampPost(s_finalPush ? postAt(15) : make_pair(50, 50));
    // 主动进军落点：距敌大本营4格（我方一侧）——不用敌中心坐标本身，
    // 敌中心大概率被武器厂等建筑占据（未侦察时 blockGrid 不标记），内核会拒绝移动
    pair<int,int> advPost = clampPost(postAt(4));
    // 方阵兵常规驻位（2026-10-08 收缩）：弓兵阵线(lineDist+4)向内(我方一侧)再缩2格。
    // 方阵兵省着用——总攻前驻守哨点不出战（诱敌由单个诱饵方阵兵执行）；
    // s_finalPush 后驻位失效，全力冲锋直插敌大本营。
    pair<int,int> guardPost = clampPost(postAt(lineDist + 6));

    // ===== 诱饵方阵兵落点（2026-10-08）：后期诱敌由单个方阵兵执行 =====
    // 前出位=距敌中心12格（守军警戒圈15格内侧、箭塔射程7格外的安全吸火位）；
    // 守军追近(<=5格)则转撤退，撤回位取 min(常规驻位, 距敌中心17格)——守军
    // 追击上限18格，撤到17格内诱其继续深入我方交战边界；守军甩开(>=9格)后
    // 再次前出，反复拉扯把守军逐股拖出箭塔保护圈，交弓兵阵线集火消耗。
    static bool s_decoyRetreat = false;
    pair<int,int> decoyDest = guardPost;
    if(allGathered && !s_finalPush && s_decoySN != -1) {
        int decoyX = -1, decoyY = -1;
        for(auto& a : info.armies) {
            if(a.SN == s_decoySN) { decoyX = a.BlockDR; decoyY = a.BlockUR; break; }
        }
        if(decoyX != -1) {
            int nearCheb = INT_MAX;
            for(auto& e : info.enemy_armies) {
                if(e.Blood <= 0) continue;
                nearCheb = min(nearCheb, max(abs(e.BlockDR - decoyX), abs(e.BlockUR - decoyY)));
            }
            if(!s_decoyRetreat && nearCheb <= 5) s_decoyRetreat = true;
            if(s_decoyRetreat && nearCheb >= 9)  s_decoyRetreat = false;
            if(s_decoyRetreat) decoyDest = clampPost(postAt(min(lineDist + 6, 17)));
            else               decoyDest = clampPost(postAt(12));
        }
    }

    // ===== 终局攻坚（2026-10-08）：敌军人类单位不可见且箭塔暴露 → 攻坚箭塔掩护祭祀 =====
    // 背景：打到后期敌军只剩箭塔（敌近战/远程=0），攻击分配整段被 enemyFound 门禁
    // 跳过，全军不发指令；箭塔持续点射重置安静计时，总攻也永远凑不满——三层死锁。
    // 触发：集结完成 + 已进入战斗后期(冲锋/弓兵进军/总攻任一) + 敌军人类单位连续
    // 150帧不可见 + 可见敌方箭塔≥1。触发后剩余兵力火力均匀轮转分配到各箭塔
    // （每单位按序号 mod 箭塔数认领），把塔打进50血触发祭祀"最终攻夺"转化武器厂。
    // 敌军人类单位再现身时暂停攻坚、由常规接敌逻辑优先处理，消失后自动恢复。
    static bool s_towerAssault = false;
    static int s_noEnemySince = -1;
    if(info.enemy_armies.empty()) {
        if(s_noEnemySince == -1) s_noEnemySince = info.GameFrame;
    } else {
        s_noEnemySince = -1;
    }
    int visibleTowerCnt = 0;
    for(auto& b : info.enemy_buildings)
        if(b.Type == BUILDING_ARROWTOWER && b.Blood > 0) visibleTowerCnt++;
    if(!s_towerAssault && allGathered && visibleTowerCnt >= 1
       && (s_chargeTriggered || bowAdvance || s_finalPush)
       && s_noEnemySince != -1 && info.GameFrame - s_noEnemySince >= 150) {
        s_towerAssault = true;
        DebugText("[phase3] 敌军人类单位不可见且箭塔暴露(" + to_string(visibleTowerCnt)
                  + "座)，发起终局攻坚：火力均分各箭塔，掩护祭祀攻夺武器厂 frame="
                  + to_string(info.GameFrame));
    }
    if(s_towerAssault && visibleTowerCnt == 0) {
        s_towerAssault = false;   // 箭塔全灭：交回常规逻辑
        DebugText("[phase3] 终局攻坚完成：敌方箭塔已全灭 frame=" + to_string(info.GameFrame));
    }

    if(finalSiegeCaptureTriggered) {
        // 最终攻夺：祭司锚点直指敌方攻城武器厂（priest() 中强制转化）
        phase3PriestTargetX = finalSiegeX; phase3PriestTargetY = finalSiegeY;
    } else {
        // 2026-10-08 新规：第三阶段祭司只待在地图中心待命，最终总攻(s_finalPush)
        // 触发后才可行动——总攻前不随军、不集结、不转化（priest() 已同步禁转化）；
        // 总攻后锚点=priestPost(距敌中心15格)，攻夺触发后直指攻城厂
        phase3PriestTargetX = priestPost.first;
        phase3PriestTargetY = priestPost.second;
    }

    // 统一移动/解卡：按兵种哨位分派目的地——方阵兵等近战→前排，复合弓兵→中排，
    // 投石车→后排；IDLE 部队走向哨位；WALKING 但连续60帧原地未动=卡脚，强制重新寻路。
    // 祭司不在此调度（由 priest() 的 phase3 分支处理跟进/转化）
    bool engagedNow = allGathered && enemyFound;
    for(auto& army : info.armies) {
        if(army.Sort == AT_PRIEST) continue;
        int destX, destY;
        if(!allGathered && army.Sort == AT_COMPOSITE_BOWMAN) { destX = bowRallyX; destY = bowRallyY; }
        else if(!allGathered)                     { destX = hopRallyX; destY = hopRallyY; }
        else if(army.Sort == AT_COMPOSITE_BOWMAN) {
            if(bowAdvance) { destX = advPost.first; destY = advPost.second; }        // 主动进军敌方大本营
            else           { destX = bowPost.first; destY = bowPost.second; }
        }
        else if(army.Sort == AT_STONE_THROWER)    { destX = stonePost.first; destY = stonePost.second; }
        else if(army.SN == s_decoySN) {
            // 诱饵方阵兵：总攻前在前出位/撤回位间拉扯诱敌；总攻后归队全力冲锋
            if(s_finalPush)       { destX = advPost.first; destY = advPost.second; }
            else                  { destX = decoyDest.first; destY = decoyDest.second; }
        }
        else {
            // 方阵兵（2026-10-08）：最终总攻前只得停留在哨点(guardPost)不得出来；
            // s_finalPush 后全力冲锋，直插敌方大本营
            if(s_finalPush)       { destX = advPost.first; destY = advPost.second; }
            else                  { destX = guardPost.first; destY = guardPost.second; }
        }
        if(engagedNow && army.SN != s_decoySN) {
            // 交战期抑制：已在前排附近(<=8格)的部队不拉回哨位——避免杀完一个目标就被
            // 拽回后方往返横跳，由下方攻击分配统一调度；身后掉队者照常赶路收拢。
            // 诱饵方阵兵不抑制：其撤退令必须随时能下达，否则被守军追上围殴
            int frontCheb = max(abs(army.BlockDR - frontPost.first), abs(army.BlockUR - frontPost.second));
            if(frontCheb <= 8) {
                lastBlock[army.SN] = {army.BlockDR, army.BlockUR};
                stuckSince.erase(army.SN);
                continue;
            }
        }
        int cheb = max(abs(army.BlockDR - destX), abs(army.BlockUR - destY));
        // 原地未动看门狗：连续10拍(300帧)块坐标不变且未到哨位——目的地大概率不可达，
        // 内核静默拒绝 HumanMove。哨位已做连通域钳制，此日志理论上不应出现，仅供排查
        auto wpIt = wdLastPos.find(army.SN);
        if(wpIt != wdLastPos.end() && wpIt->second.first == army.BlockDR
           && wpIt->second.second == army.BlockUR) {
            if(++wdSameBeats[army.SN] == 10 && cheb > 2) {
                DebugText("[phase3] 单位" + to_string(army.SN) + " 连续10拍原地未动，哨位=("
                          + to_string(destX) + "," + to_string(destY) + ") 可能不可达 frame="
                          + to_string(info.GameFrame));
            }
        } else {
            wdLastPos[army.SN] = {army.BlockDR, army.BlockUR};
            wdSameBeats[army.SN] = 0;
        }
        if(cheb <= 2) {   // 已到位：清除卡脚记录，不重复下令
            lastBlock[army.SN] = {army.BlockDR, army.BlockUR};
            stuckSince.erase(army.SN);
            continue;
        }
        bool needMove = false;
        if(army.NowState == HUMAN_STATE_IDLE) {
            needMove = true;
        } else if(army.NowState == HUMAN_STATE_WALKING) {
            auto it = lastBlock.find(army.SN);
            if(it == lastBlock.end()
               || it->second.first != army.BlockDR || it->second.second != army.BlockUR) {
                lastBlock[army.SN] = {army.BlockDR, army.BlockUR};
                stuckSince[army.SN] = info.GameFrame;
            } else if(info.GameFrame - stuckSince[army.SN] >= 60) {
                needMove = true;   // 步行中60帧原地未动：卡脚，重新 HumanMove
            }
        }
        if(needMove) HumanMove(army.SN, destX * BLOCKSIDELENGTH, destY * BLOCKSIDELENGTH);
    }

    if(!allGathered) return;   // 集结期不做攻击分配，专心赶路

    // ===== 最终攻夺掩护（2026-10-08）：触发后全军放弃攻击，人塔吸引火力 =====
    // 除祭祀外所有士兵不再发任何攻击指令，直接 HumanMove 到各敌方箭塔底下
    // （按序号轮转分配到各塔），把箭塔仇恨牢牢吸在自己身上；
    // 祭祀借掩护安全读条转化武器厂（priest() 的最终攻夺分支执行）。
    // 无可见箭塔时全员压向攻城厂周围贴身护卫祭祀。
    if(finalSiegeCaptureTriggered) {
        vector<pair<int,int>> coverTowers;
        for(auto& b : info.enemy_buildings)
            if(b.Type == BUILDING_ARROWTOWER && b.Blood > 0)
                coverTowers.push_back(make_pair(b.BlockDR, b.BlockUR));
        int ui = 0;
        for(auto& army : info.armies) {
            if(army.Sort == AT_PRIEST || army.Blood <= 0) continue;
            pair<int,int> spot;
            int holdCheb;
            if(!coverTowers.empty()) {
                pair<int,int> tp = coverTowers[ui % coverTowers.size()];
                ui++;
                spot = clampPost(make_pair(tp.first, tp.second));   // 塔底下最近可达格
                holdCheb = 1;   // 贴到塔旁1格内才停
            } else {
                spot = clampPost(make_pair(finalSiegeX, finalSiegeY));   // 攻城厂周围护卫
                holdCheb = 3;
            }
            if(max(abs(army.BlockDR - spot.first), abs(army.BlockUR - spot.second)) > holdCheb) {
                HumanMove(army.SN, spot.first * BLOCKSIDELENGTH, spot.second * BLOCKSIDELENGTH);
            }
        }
        static int coverDbgFrame = -10000;
        if(info.GameFrame - coverDbgFrame >= 125) {
            coverDbgFrame = info.GameFrame;
            int alive = 0;
            for(auto& a : info.armies)
                if(a.Sort != AT_PRIEST && a.Blood > 0) alive++;
            DebugText("[phase3攻夺掩护] 全军弃攻贴塔吸火力：敌塔" + to_string(coverTowers.size())
                      + "座 掩护兵力" + to_string(alive) + " 攻城厂=(" + to_string(finalSiegeX)
                      + "," + to_string(finalSiegeY) + ") frame=" + to_string(info.GameFrame));
        }
        return;   // 放弃攻击：塔攻坚/交战分配全部跳过，纯吸火力
    }

    // ===== 终局攻坚执行：火力均匀分配到各可见箭塔 =====
    // 敌军人类单位不可见时常规攻击分配整段被 enemyFound 门禁跳过（全军罚站），
    // 这里按单位序号 mod 箭塔数轮转认领，剩余兵力（复合弓兵/方阵兵/投石车等全部）
    // 均匀压到各箭塔上；任一塔被打进50血即触发祭祀"最终攻夺"转化武器厂。
    // 敌军人类单位再现身时本段不介入，由下方常规接敌逻辑优先处理。
    if(s_towerAssault && !enemyFound) {
        vector<int> towerSNs;
        for(auto& b : info.enemy_buildings)
            if(b.Type == BUILDING_ARROWTOWER && b.Blood > 0) towerSNs.push_back(b.SN);
        if(!towerSNs.empty()) {
            const int throwerRange = getAttackRange(AT_STONE_THROWER);
            int ui = 0;
            for(auto& army : info.armies) {
                if(army.Sort == AT_PRIEST || army.Blood <= 0) continue;
                // 方阵兵总攻前驻守哨点不参战（2026-10-08），攻坚由投石车+复合弓兵完成
                if(!s_finalPush && army.Sort == AT_HOPLITE) continue;
                int tgt;
                if(army.Sort == AT_STONE_THROWER) {
                    // 投石车原地进攻：只认领自己射程(10格)内最近的箭塔，射程外待命
                    tgt = -1;
                    int bestD = INT_MAX;
                    for(auto& b : info.enemy_buildings) {
                        if(b.Type != BUILDING_ARROWTOWER || b.Blood <= 0) continue;
                        int d = max(abs(b.BlockDR - army.BlockDR), abs(b.BlockUR - army.BlockUR));
                        if(d <= throwerRange && d < bestD) { bestD = d; tgt = b.SN; }
                    }
                    if(tgt == -1) continue;
                } else {
                    tgt = towerSNs[ui % towerSNs.size()];
                    ui++;
                }
                if(army.WorkObjectSN != tgt) {
                    HumanAction(army.SN, tgt);
                }
            }
        }
    }

    if(enemyFound) {
        // ===== 交战边界：以方阵兵前排哨位为基准 =====
        // 缓步推进期收紧(10格)：部队只索敌/集火边界内目标，严禁越界追击——
        // 上一局反攻复合弓兵越过前排追进敌方箭塔+投石车火力圈，被AOE一轮团灭。
        // 总攻期放开(40格≈全图)：方阵兵突入敌营，复合弓兵紧随其后集火。
        // (ENGAGE_DIST 已在哨位状态机前定义，与 nearThreat 判定共用)
        auto inEngage = [&](int ex, int ey) {
            return max(abs(ex - frontPost.first), abs(ey - frontPost.second)) <= ENGAGE_DIST;
        };

        // 集火目标：交战边界内血量最低的敌人（并列取先遍历到的）
        int focusTarget = -1, minBlood = INT_MAX;
        for(auto& enemy : info.enemy_armies) {
            if(enemy.Blood <= 0) continue;
            if(!inEngage(enemy.BlockDR, enemy.BlockUR)) continue;
            if(enemy.Blood < minBlood) {
                minBlood = enemy.Blood;
                focusTarget = enemy.SN;
            }
        }

        // 近战目标：交战边界内离前排哨位最近的近战敌人（攻击距离<=1）；
        // 敌方守军追出大本营时会先进入边界，方阵兵在前排接住近战；
        // 无近战敌人时退化为集火目标，保证近战兵种始终有输出
        int meleeTarget = -1, minFrontCheb = INT_MAX;
        for(auto& enemy : info.enemy_armies) {
            if(enemy.Blood <= 0) continue;
            if(getAttackRange(enemy.Sort) > 1) continue;   // 只打近战敌人
            if(!inEngage(enemy.BlockDR, enemy.BlockUR)) continue;
            int cheb = max(abs(enemy.BlockDR - frontPost.first), abs(enemy.BlockUR - frontPost.second));
            if(cheb < minFrontCheb) { minFrontCheb = cheb; meleeTarget = enemy.SN; }
        }
        if(meleeTarget == -1) meleeTarget = focusTarget;

        // 本拍已下指令（攻击/风筝撤退）的部队，追击切断不再重复调度
        set<int> orderedThisBeat;

        // ===== 复合弓兵：后排集火 + 拉扯风筝（参考玩家手动微操） =====
        for(auto& army : info.armies) {
            if(army.Sort != AT_COMPOSITE_BOWMAN || army.Blood <= 0) continue;
            // 1) 风筝优先：贴脸近战(切比雪夫<=4)→背离该敌人退5格，本拍不下攻击指令；
            //    下一拍距离已拉开继续输出，避免近战贴脸白嫖高价值复合弓兵
            int chaserX = 0, chaserY = 0; bool chased = false;
            for(auto& enemy : info.enemy_armies) {
                if(enemy.Blood <= 0 || getAttackRange(enemy.Sort) > 1) continue;
                int cheb = max(abs(enemy.BlockDR - army.BlockDR), abs(enemy.BlockUR - army.BlockUR));
                if(cheb <= 4) { chased = true; chaserX = enemy.BlockDR; chaserY = enemy.BlockUR; break; }
            }
            if(chased) {
                int dx = army.BlockDR - chaserX, dy = army.BlockUR - chaserY;
                if(dx == 0 && dy == 0) dx = 1;
                double len = sqrt((double)dx * dx + (double)dy * dy);
                int fx = army.BlockDR + (int)(dx / len * 5 + (dx >= 0 ? 0.5 : -0.5));
                int fy = army.BlockUR + (int)(dy / len * 5 + (dy >= 0 ? 0.5 : -0.5));
                fx = max(0, min(MAP_SIZE - 1, fx));
                fy = max(0, min(MAP_SIZE - 1, fy));
                pair<int,int> spot = legalPlaceAround(114514, fx, fy);
                if(spot.first != -1)
                    HumanMove(army.SN, spot.first * BLOCKSIDELENGTH, spot.second * BLOCKSIDELENGTH);
                orderedThisBeat.insert(army.SN);
                continue;
            }
            // 2) 集火：全局集火目标在该弓兵射程内(9格)则统一射它；
            //    否则射边界内9格内最近的敌人（不追出射程圈，保持后排站位）
            int desired = -1;
            if(focusTarget != -1) {
                for(auto& enemy : info.enemy_armies) {
                    if(enemy.SN != focusTarget || enemy.Blood <= 0) continue;
                    if(max(abs(enemy.BlockDR - army.BlockDR), abs(enemy.BlockUR - army.BlockUR)) <= 9)
                        desired = focusTarget;
                    break;
                }
            }
            if(desired == -1) {
                int best = INT_MAX;
                for(auto& enemy : info.enemy_armies) {
                    if(enemy.Blood <= 0) continue;
                    if(!inEngage(enemy.BlockDR, enemy.BlockUR)) continue;
                    int cheb = max(abs(enemy.BlockDR - army.BlockDR), abs(enemy.BlockUR - army.BlockUR));
                    if(cheb <= 9 && cheb < best) { best = cheb; desired = enemy.SN; }
                }
            }
            if(desired != -1 && army.WorkObjectSN != desired) {   // 已在打该目标则不重发，避免重置攻击循环
                HumanAction(army.SN, desired);
                orderedThisBeat.insert(army.SN);
            }
        }

        // ===== 其它近战兵种（方阵兵/阔剑兵等）=====
        // 2026-10-08 调整：最终总攻(s_finalPush)前方阵兵只得停留在哨点不得出来——
        // 不下发任何攻击指令（诱敌由单个诱饵方阵兵执行），避免被仇恨拉出白给。
        // s_finalPush 后全力冲锋，集中火力攻击 chargeTarget——
        //   首要目标：敌方远程单位（含投石车等攻城器械），取离前排最近；
        //   次要目标：敌方远程全灭后，锁定距离最近的敌方箭塔；
        //   两者皆无（敌军已清空）→ 退回常规近战接敌，保证始终有输出。
        // 冲锋目标不受交战边界限制（追进大本营歼灭远程火力）。
        if(s_finalPush) {
            int chargeTarget = -1;
            int bestCheb = INT_MAX;
            for(auto& e : info.enemy_armies) {
                if(e.Blood <= 0 || getAttackRange(e.Sort) <= 1) continue;   // 只打远程
                int cheb = max(abs(e.BlockDR - frontPost.first), abs(e.BlockUR - frontPost.second));
                if(cheb < bestCheb) { bestCheb = cheb; chargeTarget = e.SN; }
            }
            if(chargeTarget == -1) {
                int bestD = INT_MAX;
                for(auto& b : info.enemy_buildings) {
                    if(b.Type != BUILDING_ARROWTOWER || b.Blood <= 0) continue;
                    int d = max(abs(b.BlockDR - frontPost.first), abs(b.BlockUR - frontPost.second));
                    if(d < bestD) { bestD = d; chargeTarget = b.SN; }
                }
            }
            int meleeOrder = (chargeTarget != -1) ? chargeTarget : meleeTarget;
            if(meleeOrder != -1) {
                for(auto& army : info.armies) {
                    if(army.Sort == AT_PRIEST || army.Sort == AT_COMPOSITE_BOWMAN
                       || army.Sort == AT_STONE_THROWER) continue;
                    if(army.Blood <= 0) continue;
                    if(army.WorkObjectSN != meleeOrder) {
                        HumanAction(army.SN, meleeOrder);
                        orderedThisBeat.insert(army.SN);
                    }
                }
            }
        }

        // ===== 投石车（2026-10-08：原地进攻）=====
        // 只攻击自己当前射程(10格)内的目标——箭塔优先(取最近)，其次敌方投石车；
        // 射程外不追击不前压，站在弓兵阵线原地输出。
        // 严禁攻击箭塔以外的建筑——AOE溅射会误伤待转化的攻城武器厂，破坏 isWin 胜利条件
        {
            const int throwerRange = getAttackRange(AT_STONE_THROWER);
            for(auto& army : info.armies) {
                if(army.Sort != AT_STONE_THROWER || army.Blood <= 0) continue;
                int tgt = -1, bestD = INT_MAX;
                for(auto& b : info.enemy_buildings) {
                    if(b.Type != BUILDING_ARROWTOWER || b.Blood <= 0) continue;
                    int d = max(abs(b.BlockDR - army.BlockDR), abs(b.BlockUR - army.BlockUR));
                    if(d <= throwerRange && d < bestD) { bestD = d; tgt = b.SN; }
                }
                if(tgt == -1) {
                    for(auto& e : info.enemy_armies) {
                        if(e.Blood <= 0 || e.Sort != AT_STONE_THROWER) continue;
                        int d = max(abs(e.BlockDR - army.BlockDR), abs(e.BlockUR - army.BlockUR));
                        if(d <= throwerRange && d < bestD) { bestD = d; tgt = e.SN; }
                    }
                }
                if(tgt != -1 && army.WorkObjectSN != tgt) {
                    HumanAction(army.SN, tgt);
                    orderedThisBeat.insert(army.SN);
                }
            }
        }

        // ===== 追击切断：本拍未下令、但工作对象指向边界外敌人/敌建筑的部队拉回哨位 =====
        // （内核自动追击/旧目标残留会把部队拖进敌方火力圈；击杀达标/进军/总攻期放开不切断）
        if(!s_finalPush && !s_chargeTriggered && !bowAdvance) {
            for(auto& army : info.armies) {
                if(army.Sort == AT_PRIEST || army.Sort == AT_STONE_THROWER) continue;
                if(army.Blood <= 0 || army.WorkObjectSN <= 0) continue;
                if(orderedThisBeat.count(army.SN)) continue;
                bool outside = false;
                for(auto& enemy : info.enemy_armies) {
                    if(enemy.SN != army.WorkObjectSN) continue;
                    if(max(abs(enemy.BlockDR - frontPost.first), abs(enemy.BlockUR - frontPost.second)) > ENGAGE_DIST + 2)
                        outside = true;
                    break;
                }
                if(!outside) {
                    for(auto& b : info.enemy_buildings) {
                        if(b.SN != army.WorkObjectSN) continue;
                        if(max(abs(b.BlockDR - frontPost.first), abs(b.BlockUR - frontPost.second)) > ENGAGE_DIST + 2)
                            outside = true;
                        break;
                    }
                }
                if(outside) {
                    int px = guardPost.first, py = guardPost.second;   // 回方阵兵常规驻位（弓兵阵线内侧）
                    if(army.Sort == AT_COMPOSITE_BOWMAN) { px = bowPost.first; py = bowPost.second; }
                    HumanMove(army.SN, px * BLOCKSIDELENGTH, py * BLOCKSIDELENGTH);
                }
            }
        }
    }
    // 视野内无敌军时无需攻击分配：部队驻守各自哨位，推进/后撤/总攻由哨位状态机统一调度

    // ===== 战斗调试输出：每5秒(125帧)一次 =====
    // 状态行：冲锋/进军/攻坚/最终攻夺等触发判定 + 决策参数；单位行：全体兵种-状态-坐标-目标。
    // 拆成两条 DebugText 输出，避免单条过长被日志截断。
    static int lastCombatDbgFrame = -10000;
    if(allGathered && info.GameFrame - lastCombatDbgFrame >= 125) {
        lastCombatDbgFrame = info.GameFrame;
        int towerCnt = 0, minTowerBlood = INT_MAX, rangedCnt = 0, meleeCnt = 0;
        for(auto& b : info.enemy_buildings) {
            if(b.Type == BUILDING_ARROWTOWER && b.Blood > 0) {
                towerCnt++;
                minTowerBlood = min(minTowerBlood, b.Blood);
            }
        }
        for(auto& e : info.enemy_armies) {
            if(e.Blood <= 0) continue;
            if(getAttackRange(e.Sort) > 1) rangedCnt++; else meleeCnt++;
        }
        string units;
        for(auto& a : info.armies) {
            string nm = a.Sort == AT_HOPLITE ? "方阵兵"
                      : a.Sort == AT_COMPOSITE_BOWMAN ? "复合弓兵"
                      : a.Sort == AT_STONE_THROWER ? "投石车"
                      : a.Sort == AT_PRIEST ? "祭司" : "兵" + to_string(a.Sort);
            units += nm + "#" + to_string(a.SN) + "(st" + to_string(a.NowState)
                   + "@" + to_string(a.BlockDR) + "," + to_string(a.BlockUR)
                   + ",tgt" + to_string(a.WorkObjectSN) + ") ";
        }
        DebugText("[phase3战斗] 击杀达标=" + string(s_chargeTriggered ? "是" : "否")
                  + " 近战击杀=" + to_string(meleeKills) + "(>25达标)"
                  + " 弓兵进军=" + string(bowAdvance ? "是" : "否")
                  + " 无近战平静帧=" + to_string(bowCalmSince == -1 ? 0 : info.GameFrame - bowCalmSince)
                  + "(375触发) 总攻=" + string(s_finalPush ? "是" : "否")
                  + " 塔攻坚=" + string(s_towerAssault ? "是" : "否")
                  + " 最终攻夺=" + string(finalSiegeCaptureTriggered ? "已触发" : "未触发")
                  + " 敌方大本营=(" + to_string(ecx) + "," + to_string(ecy) + ")"
                  + " 敌近战=" + to_string(meleeCnt) + " 敌远程=" + to_string(rangedCnt)
                  + " 敌塔=" + to_string(towerCnt) + "塔最低血="
                  + (minTowerBlood == INT_MAX ? string("无") : to_string(minTowerBlood))
                  + " 塔已毁=" + to_string(towersDestroyed) + "(3触发攻夺,"
                  + string(priestTowerUnlocked ? "已解锁" : "锁定") + ")"
                  + " 祭司=" + string(finalSiegeCaptureTriggered ? "攻夺中"
                            : phase3FinalPush ? "总攻随军" : "中心待命"));
        DebugText("[phase3单位] " + units);
    }
}

void UsrAI::assignFarmer()
{
    static int gamePhase1Over = 0;
    static int gamePhase2Over = 0;
    // 农民人口上限：gamePhase1 为 20，gamePhase2 起为 25（只统计农民，不含军队）
    if(!gamePhase1Over && info.civilizationStage <= CIVILIZATION_TOOLAGE) {
        HumanControl = 19;
        gamePhase1();
    } else if(!gamePhase2Over && checkArmy1()) {
        gamePhase1Over = 1;
        HumanControl = 19;
        gamePhase2();
    } else {
        gamePhase2Over = 1;
        gamePhase3();
    }
}



// 建筑所需时代
int UsrAI::getBuildingRequiredAge(int buildingType)
{
    switch (buildingType) {
        case BUILDING_HOME:       // 房屋
        case BUILDING_GRANARY:    // 谷仓
        case BUILDING_STOCK:      // 仓库
        case BUILDING_ARMYCAMP:   // 兵营
            return CIVILIZATION_STONEAGE;
        case BUILDING_MARKET:     // 市场
        case BUILDING_RANGE:      // 靶场
        case BUILDING_STABLE:     // 马厩
        case BUILDING_FARM:       // 农场
        case BUILDING_ARROWTOWER: // 箭塔
            return CIVILIZATION_TOOLAGE;
        default:
            return CIVILIZATION_BRONZEAGE;
    }
}

// 建筑木材消耗
int UsrAI::getBuildingWoodCost(int buildingType)
{
    switch (buildingType) {
        case BUILDING_HOME:       return 30;
        case BUILDING_GRANARY:    return 120;
        case BUILDING_STOCK:      return 120;
        case BUILDING_ARMYCAMP:   return 125;
        case BUILDING_MARKET:     return 150;
        case BUILDING_RANGE:      return 150;
        case BUILDING_STABLE:     return 150;
        case BUILDING_COLLAGE:    return 180;
        case BUILDING_FARM:       return 75;
        case BUILDING_ARROWTOWER: return 0;    // 箭塔只用石头
        default:                  return 0;
    }
}

// 建筑石头消耗（仅箭塔）
int UsrAI::getBuildingStoneCost(int buildingType)
{
    switch (buildingType) {
        case BUILDING_ARROWTOWER: return 150;
        default:                  return 0;
    }
}

// 检查时代与资源是否满足建造条件
bool UsrAI::checkResource(int buildingType)
{
    if(info.civilizationStage < getBuildingRequiredAge(buildingType))
        return false;   // 时代未达到
    if(info.Wood < getBuildingWoodCost(buildingType))
        return false;   // 木材不足
    if(info.Stone < getBuildingStoneCost(buildingType))
        return false;   // 石头不足
    return true;
}

void UsrAI::arrowTowerAttack(int towerSN, int BlockDR, int BlockUR)
{
    //DBG_OFF("current towerSN = " + to_string(towerSN));
    //static map<int, bool > haveAttacked;
    static map<int, int > lastAttackFrame;
    const int attackRange = 7;
    const int attackInterval = 40;

    if(info.GameFrame - lastAttackFrame[towerSN] < attackInterval) return;
    lastAttackFrame[towerSN] = info.GameFrame;

    // 索敌策略：优先攻击"目标不是本塔"的敌人；若所有敌军都打本塔则不切换
    int nextEnemySN = -1;

    for(auto& enemy: info.enemy_armies) {
        if(enemy.Blood <= 0) continue;
        // 2026-10-08 更新：投石车不再保留给祭祀转化（部队可直接射杀），原禁攻规则废除
        // 祭祀转化目标保护（2026-10-08）：祭祀锁定且未超时(45秒)的目标不得射杀，
        // 留给祭祀转化（含祭祀20秒休整期），避免转化效率被箭塔火力损耗
        if(enemy.SN == priestReserveSN && info.GameFrame - priestReserveFrame < 1800) continue;
        if(abs(BlockDR - enemy.BlockDR) <= attackRange && abs(BlockUR - enemy.BlockUR) <= attackRange) {
            // 找到目标不是本塔的敌人，优先攻击它（吸引仇恨到箭塔，保护祭司）
            if(enemy.WorkObjectSN != towerSN) {
                nextEnemySN = enemy.SN;
                break;
            }
        }
    }

    // 所有射程内敌军的目标都是本塔 → 不切换目标，不发命令（内核自动持续攻击当前目标）
    if(nextEnemySN == -1) return;

    HumanAction(towerSN, nextEnemySN);
}

void UsrAI::assignArmy()
{
    for(auto& building: info.buildings) {
        if(building.Type == BUILDING_ARROWTOWER) {
            arrowTowerAttack(building.SN, building.BlockDR, building.BlockUR);
        }
    }

    // 第三阶段反攻后，军队的移动/攻击目标由 gamePhase3 统一调度，
    // 此处的家中防守索敌不再介入（否则会与集结/推进的 HumanMove 同帧打架导致半路停下）
    if(phase3Active) return;

    // 第二波消灭后（13500帧之后且无敌人）：士兵围绕祭司身边集结守护，直到第三阶段触发
    static bool secondWaveCleared = false;
    static int lastGuardOrderFrame = -10000;
    if(info.GameFrame > 13500) {
        secondWaveCleared = info.enemy_armies.empty();
    }
    if(secondWaveCleared) {
        int priestBX = -1, priestBY = -1;
        for(auto& army : info.armies) {
            if(army.SN == priestSN) { priestBX = army.BlockDR; priestBY = army.BlockUR; break; }
        }
        if(priestBX >= 0 && info.GameFrame - lastGuardOrderFrame >= 30) {
            lastGuardOrderFrame = info.GameFrame;
            for(auto& army : info.armies) {
                if(army.SN == priestSN) continue;
                int cheb = max(abs(army.BlockDR - priestBX), abs(army.BlockUR - priestBY));
                if(cheb > 4) {
                    pair<int,int> spot = legalPlaceAround(114514, priestBX, priestBY);
                    if(spot.first != -1) {
                        HumanMove(army.SN, spot.first * BLOCKSIDELENGTH, spot.second * BLOCKSIDELENGTH);
                    }
                }
            }
        }
        return;
    }

    // 优先级：正在攻击祭祀的敌人 > 与自身不同兵种(近战/远程)的敌人 > 其他敌人；同层级内取最近
    for(auto& army : info.armies) {
        if(army.SN == priestSN || army.NowState != HUMAN_STATE_IDLE) continue;
        bool armyIsMelee = (getAttackRange(army.Sort) <= 1);
        int targetSN = -1;
        int bestTier = 3;       // 1=攻击祭祀 2=不同兵种 3=其他
        int bestDist = INT_MAX;
        for(auto& enemy : info.enemy_armies) {
            // 2026-10-08 更新：投石车不再保留给祭祀转化，任何我方单位可直接进攻
            // 祭祀转化目标保护（2026-10-08）：留给祭祀转化，同箭塔规则（45秒超时）
            if(enemy.SN == priestReserveSN && info.GameFrame - priestReserveFrame < 1800) continue;
            int tier;
            if(enemy.WorkObjectSN == priestSN) {
                tier = 1;                                        // 正在攻击祭祀，最高优先
            } else {
                bool enemyIsMelee = (getAttackRange(enemy.Sort) <= 1);
                tier = (armyIsMelee != enemyIsMelee) ? 2 : 3;    // 不同兵种次之，其余再次
            }
            int d = manhattan_distance(enemy.BlockDR, enemy.BlockUR, army.BlockDR, army.BlockUR);
            if(tier < bestTier || (tier == bestTier && d < bestDist)) {
                bestTier = tier;
                bestDist = d;
                targetSN = enemy.SN;
            }
        }
        if(targetSN != -1) {
            HumanAction(army.SN, targetSN);
        }
    }
}

// 浆果采集维护：开局 6 名浆果村民（init() 预插入 berryFarmersSN）一对一采集，
// 采完自己的浆果改派其它开局可见浆果；所有浆果枯竭后释放并直接进伐木组
// （treeFarmersSN：phase1 由 logging 认领，phase2 交替分配循环会跳过伐木组成员）。
// 注意：新生村民由 gamePhase1 按 伐木:采肉=2:1 分配，第 3 人进猎人组
// （hunting()/collecting() 的羚羊肉采集体系），不再进浆果组——开局 6 颗浆果
// 已被 6 名初始村民占满，新浆果工无果可采。
void UsrAI::berryCollecting() {
    // 开局一次性记录所有浆果资源 SN
    if(berryResourceSNs.empty()) {
        for(auto& r : info.resources) {
            if(r.Type == RESOURCE_BUSH) berryResourceSNs.push_back(r.SN);
        }
        if(berryResourceSNs.empty()) return;  // 地图无浆果
    }

    // 当前仍可采集的浆果集合（Cnt>0）
    set<int> available;
    for(auto& r : info.resources) {
        if(r.Type == RESOURCE_BUSH && r.Cnt > 0) available.insert(r.SN);
    }

    // 释放已耗尽浆果的占用
    for(int sn : berryResourceSNs) {
        if(available.count(sn) == 0) assignedBerrySNs.erase(sn);
    }

    // 既有浆果农民维护：中断后安全网重发；自己的浆果采完后改派其它开局可见浆果，
    // 所有浆果枯竭后才释放交回 logging 收编（需求1：持续采集所有开局可见浆果）
    for(auto& farmer : info.farmers) {
        if(berryFarmersSN.count(farmer.SN) == 0) continue;
        if(farmer.NowState != HUMAN_STATE_IDLE) continue;
        auto it = berryAssignment.find(farmer.SN);
        int curSN = (it == berryAssignment.end()) ? -1 : it->second;
        // 当前浆果仍可采：安全网重发采集指令
        if(curSN != -1 && available.count(curSN)) {
            HumanAction(farmer.SN, curSN);
            continue;
        }
        if(curSN != -1) {
            assignedBerrySNs.erase(curSN);
            berryAssignment.erase(it);
        }
        // 改派到其它未占用的开局可见浆果
        int berrySN = -1;
        for(int sn : berryResourceSNs) {
            if(available.count(sn) && assignedBerrySNs.count(sn) == 0) { berrySN = sn; break; }
        }
        if(berrySN != -1) {
            assignedBerrySNs.insert(berrySN);
            berryAssignment[farmer.SN] = berrySN;
            HumanAction(farmer.SN, berrySN);
        } else {
            // 所有浆果全部枯竭，释放该农民并直接进伐木组（浆果村民采完全部砍树）
            berryFarmersSN.erase(farmer.SN);
            treeFarmersSN.insert(farmer.SN);
        }
    }
}

// 需求4：采石逻辑仿照 logging，按距离仓库/市镇中心远近排序，为采石村民分配最近采石点
void UsrAI::stoneMining() {
    if(stoneFarmersSN.empty()) return;

    static bool flag = false;              // 是否已完成首次分配：首次进入立即切换，之后做卡脚检测
    static map<int,int> farmerState;       // 村民上一帧状态，用于卡脚检测

    priority_queue<pair<int,int>, vector<pair<int,int>>, greater<pair<int,int>>> candStoneSNs;
    const int INF = 1e9;
    for(auto& r : info.resources) {
        if(r.Type != RESOURCE_STONE || r.Cnt <= 0) continue;
        int minDist = INF;
        for(auto& b : info.buildings) {
            if(b.Type == BUILDING_CENTER || b.Type == BUILDING_STOCK) {
                minDist = min(minDist, manhattan_distance(b.BlockDR, b.BlockUR, r.BlockDR, r.BlockUR));
            }
        }
        if(minDist != INF) candStoneSNs.push(make_pair(minDist, r.SN));
    }
    if(candStoneSNs.empty()) return;

    for(auto& farmer : info.farmers) {
        if(stoneFarmersSN.count(farmer.SN) == 0) continue;
        // 首次进入立即切换；之后仅在村民卡脚(连续两帧都空闲)时重新指派
        if(!flag || (farmerState[farmer.SN] == HUMAN_STATE_IDLE && farmer.NowState == HUMAN_STATE_IDLE)) {
            if(candStoneSNs.empty()) break;
            HumanAction(farmer.SN, candStoneSNs.top().second);
            candStoneSNs.pop();
        }
        farmerState[farmer.SN] = farmer.NowState;
    }

    flag = true;
}

// resourceSwitch：手上木头足够建造军营+市场+马厩各一个（已建好的相应扣减）时触发；
// 重构后动作：伐木村民对半分——一半继续砍树，一半转入猎人组帮助采集羚羊尸体
// （hunting()/collecting() 模块不变，猎人组由其统一管理狩猎与收尸）
void UsrAI::resourceSwitch() {
    if(resourceSwitchDone) return;

    // 触发条件：木头 >= (军营/市场/马厩中未建成者的木材造价之和)
    int needWood = 0;
    bool hasCamp = false, hasMarket = false, hasStable = false;
    for(auto& b : info.buildings) {
        if(b.Type == BUILDING_ARMYCAMP) hasCamp = true;
        else if(b.Type == BUILDING_MARKET) hasMarket = true;
        else if(b.Type == BUILDING_STABLE) hasStable = true;
    }
    if(!hasCamp)   needWood += getBuildingWoodCost(BUILDING_ARMYCAMP);
    if(!hasMarket) needWood += getBuildingWoodCost(BUILDING_MARKET);
    if(!hasStable) needWood += getBuildingWoodCost(BUILDING_STABLE);
    if(info.Wood < needWood) return;
    resourceSwitchDone = true;

    // 收集当前伐木村民：
    //   phase2（unifiedAssignDone）→ treeFarmersSN 成员；
    //   phase1 → 未被任何工种标记的非 builder 村民（logging() 正在管理的伐木工）
    vector<int> treeList;
    if(unifiedAssignDone) {
        treeList.assign(treeFarmersSN.begin(), treeFarmersSN.end());
    } else {
        for(auto& f : info.farmers) {
            int sn = f.SN;
            if(sn == builderSN || sn == waitingHunterSN || sn == pairingFarmerSN) continue;
            if(berryFarmersSN.count(sn) || hunterFarmersSN.count(sn)) continue;
            if(stoneFarmersSN.count(sn) || repairFarmersSN.count(sn)) continue;
            if(goldFarmersSN.count(sn) || currentFarmersSN.count(sn)) continue;
            treeList.push_back(sn);
        }
    }

    // 对半分：前一半转猎人组帮助采集羚羊尸体，后一半继续砍树（进 treeFarmersSN 统一记账）
    int moveCnt = (int)treeList.size() / 2;
    string movedList, keptList;
    for(int i = 0; i < (int)treeList.size(); i++) {
        int sn = treeList[i];
        if(i < moveCnt) {
            hunterFarmersSN.insert(sn);
            corpseHelperSNs.insert(sn);   // 登记为收尸帮工：collecting() 收尸时均分到各羚羊尸体
            treeFarmersSN.erase(sn);
            movedList += to_string(sn) + " ";
        } else {
            treeFarmersSN.insert(sn);
            keptList += to_string(sn) + " ";
        }
    }
    DBG_OFF("[switchDBG] resourceSwitch frame=" + to_string(info.GameFrame)
              + " needWood=" + to_string(needWood)
              + " wood=" + to_string(info.Wood)
              + " ->hunters={" + movedList + "} keepTree={" + keptList + "}");

    // resourceSwitch 后优先安排木材加工升级（研发:远程攻击距离+1,伐木+2；120食物/75木头）
    woodUpgradePending = true;
}

// 木材加工升级（市场）：resourceSwitch 后优先下发；30 帧节流 + 市场空闲 + ins_ret 判定成功
void UsrAI::woodUpgrade() {
    static int woodOrderId = -1;
    static int woodOrderFrame = -10000;

    if(!woodUpgradePending) return;

    // 上一条指令结果（ins_ret 仅下一帧有效），读到即复位
    if(woodOrderId != -1) {
        auto it = info.ins_ret.find(woodOrderId);
        if(it != info.ins_ret.end()) {
            DBG_OFF("[woodDBG] Wood upgrade ins_ret code = " + to_string(it->second)
                      + " frame = " + to_string(info.GameFrame));
            if(it->second == ACTION_SUCCESS) {
                woodUpgradePending = false;   // 研究已开始，永久停止
                return;
            }
            woodOrderId = -1;   // 失败：下一窗口重试
        }
    }

    if(info.GameFrame - woodOrderFrame < 30) return;
    woodOrderFrame = info.GameFrame;

    for(auto& building : info.buildings) {
        if(building.Type == BUILDING_MARKET && building.Project == ACT_NULL
            && info.Meat >= BUILDING_MARKET_WOOD_UPGRADE_FOOD
            && info.Wood >= BUILDING_MARKET_WOOD_UPGRADE_WOOD) {
            woodOrderId = BuildingAction(building.SN, BUILDING_MARKET_WOOD_UPGRADE);
            DBG_OFF("[woodDBG] Wood upgrade ordered, frame = " + to_string(info.GameFrame));
            break;
        }
    }
}

// 需求5：14000帧（第二波敌人进攻阶段）采石村民二次分配：1人继续采石，其余转箭塔维修
void UsrAI::repairRepurpose() {
    if(repairRepurposeDone) return;
    if(info.GameFrame < 13500) return;
    repairRepurposeDone = true;

    // 从采石村民中选前1人继续采石，其余转维修
    vector<int> stoneList(stoneFarmersSN.begin(), stoneFarmersSN.end());
    const int keepStoneNum = 1;
    for(int i = keepStoneNum; i < (int)stoneList.size(); i++) {
        stoneFarmersSN.erase(stoneList[i]);
        repairFarmersSN.insert(stoneList[i]);
    }
    // 调试：二次分配结果
    string snList;
    for(int sn : repairFarmersSN) snList += to_string(sn) + " ";
    DBG_OFF("[repairDBG] repairRepurpose @13500: repairFarmers={" + snList + "}");
}

void UsrAI::processData()
{
    init();

    updateTech();

    assignArmy();
    priest();

    // 开局配对：待命村民 + 市镇中心首个新村民 → 一起打猎
    // 必须在 resourceSwitch 之前执行，防止切换逻辑把待配对的新村民当伐木工分走
    pairFirstHunters();
    // 资源动态切换：木头够建军营+市场+马厩 → 伐木村民对半分（一半继续砍树 / 一半帮收羚羊尸体）
    resourceSwitch();
    // resourceSwitch 后优先安排木材加工升级（远程攻击距离+1,伐木+2）
    woodUpgrade();
    // 需求5：13500帧采石村民二次分配（1人采石 / 其余修塔）
    repairRepurpose();

    // 优先修箭塔：builder 始终 + 二次分配来的维修组，血量最低塔优先
    // 放在 assignFarmer/assignBuilding 之前，确保维修不被建造抢占
    fixingArrowTower();
    // 需求4：采石村民分配最近采石点
    stoneMining();

    assignFarmer();
    farmingIdlePoll();   // 1s轮询：卡住空闲的耕作村民重新种地（放在分派之后，仅对IDLE生效不冲突）
    assignBuilding();
    fixUnstaffedFoundations();   // 看护无人修建的未完工地基（建筑工被抽走时补人，防99%搁置）

    createFarmer();
}

//farming()未触发