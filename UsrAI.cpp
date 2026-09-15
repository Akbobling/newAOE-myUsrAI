#include "UsrAI.h"
#include "config.h"
#include <set>
#include <iostream>
#include <unordered_map>
#include <list>
#include <cstdlib>
#include <algorithm>
#include <climits>

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
int HumanControl = 10;
int farmNum = 0;
int farmDx[8] = {0, 0, -4, -4, -4, 4, 4, 4};
int farmDy[8] = {-4, 4, -3, 0, 3, -4, 0, 3};
vector<int> deadMeatResources;
set<int> currentFarmersSN;
set<int> arrowTowerSN;

// ==== 浆果采集 / 资源动态切换状态 ====
vector<int> berryResourceSNs;      // 开局记录的浆果资源 SN
set<int> assignedBerrySNs;         // 已分配给村民的浆果 SN（一对一）
map<int,int> berryAssignment;      // 村民SN -> 浆果SN
set<int> hunterFarmersSN;          // 猎人（开局初始村民，狩猎羚羊）
set<int> berryFarmersSN;           // 浆果采集村民（新生成村民）
set<int> stoneFarmersSN;           // 采石村民
set<int> repairFarmersSN;          // 箭塔维修村民（14000帧后从采石组二次分配而来）
set<int> goldFarmersSN;            // 采金村民（gamePhase2 统一分配后的主力采集组）
set<int> treeFarmersSN;            // 伐木村民（供应房屋/军队/科技木材；farming 每建一块田临时抽调一人）
bool unifiedAssignDone = false;    // gamePhase2 是否已执行统一分配
bool hunterInitialized = false;    // 猎人组是否已初始化
bool resourceSwitchDone = false;   // 需求4：食物/人口达标后切采石+伐木
bool repairRepurposeDone = false;  // 需求5：14000帧采石村民二次分配
bool compositeBowDone = false;    // 复合弓科技是否升级成功（ins_ret == ACTION_SUCCESS）
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

    for(auto& farmer: info.farmers) {
        if(builderSN == -1) {
            builderSN = farmer.SN;
            break;
        }
    }

    // 需求2/3：开局首次将所有现有村民（除 builder）初始化为猎人
    // 之后新生成的村民由 berryCollecting() 认领为浆果采集者
    if(!hunterInitialized) {
        for(auto& farmer: info.farmers) {
            if(farmer.SN != builderSN) {
                hunterFarmersSN.insert(farmer.SN);
            }
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
            DebugText("BronzeAge upgrade ins_ret code = " + to_string(it->second)
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
                DebugText("Action");
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
    static int lastPriestBlood = -1;     // 上帧祭祀血量，用于检测受攻击
    const int retryInterval = 50;
    const int radius = 5;

    int priestX = -1, priestY = -1, priestBlood = -1;
    for(auto& army: info.armies) {
        if(army.Sort == AT_PRIEST) {
            priestX = army.BlockDR;
            priestY = army.BlockUR;
            priestBlood = army.Blood;
            break;
        }
    }
    if(priestX == -1) return;

    //DebugText("lastOrderX = " + to_string(lastOrderX) + ", lastOrderY = " + to_string(lastOrderY));
    //DebugText("manhattan_distance = " + to_string(manhattan_distance(lastOrderX,lastOrderY,priestX,priestY)))
    // 需求7：转化保护——转化中且未受攻击(Blood未减少)时，暂停移动逻辑（全帧生效）
    if(converting) {
        bool targetExists = false;
        for(auto& enemy : info.enemy_armies) {
            if(enemy.SN == convertTargetSN) { targetExists = true; break; }
        }
        if(targetExists && priestBlood != -1 && lastPriestBlood != -1 && priestBlood >= lastPriestBlood) {
            return;
        }
        converting = false;
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
            for(auto& enemy : info.enemy_armies) {
                if(enemy.SN == convertTargetSN && enemy.Blood > 0) { targetAlive = true; break; }
            }
            if(convertTargetSN != -1 && !targetAlive) {
                convertTargetSN = -1;   // 转化成功或目标阵亡，解锁换目标
            }

            if(convertTargetSN == -1) {
                // 收集我方友军（箭塔 + 我方单位/军队）SN 集合
                set<int> myFriendSNs;
                for(auto& b : info.buildings) {
                    if(b.Blood > 0) myFriendSNs.insert(b.SN);   // 所有友方建筑（含箭塔）
                }
                for(auto& army : info.armies) {
                    if(army.Blood > 0) myFriendSNs.insert(army.SN);   // 我方军队
                }
                // 两轮筛选：仇恨已在友军身上（含箭塔）的前提下，优先选择最近的方阵兵(AT_HOPLITE)转化；
                // 无方阵兵时退化为选择最近的满足仇恨条件的任意敌军单位
                int minDistPhalanx = INT_MAX, minDistAny = INT_MAX;
                int phalanxSN = -1, anySN = -1;
                for(auto& enemy : info.enemy_armies) {
                    if(enemy.Blood <= 0) continue;
                    if(myFriendSNs.find(enemy.WorkObjectSN) == myFriendSNs.end()) continue;   // 仇恨必须在友军（含箭塔）身上
                    int d = manhattan_distance(enemy.BlockDR, enemy.BlockUR, priestX, priestY);
                    if(enemy.Sort == AT_HOPLITE) {
                        if(d < minDistPhalanx) { minDistPhalanx = d; phalanxSN = enemy.SN; }
                    } else {
                        if(d < minDistAny) { minDistAny = d; anySN = enemy.SN; }
                    }
                }
                convertTargetSN = (phalanxSN != -1) ? phalanxSN : anySN;
            }

            if(convertTargetSN != -1) {
                HumanAction(priestSN, convertTargetSN);  // 转化（内核 ATTACKTYPE_CHANGE）
                converting = true;
                nextX = priestX;  // 未移动，占位避免 lastOrder 记录越界
                nextY = priestY;
            } else {
                // 没有可转化方阵兵，退回原逻辑：移动到箭塔中心
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
                    DebugText("No arrow tower");
                    return;
                }
                midX /= arrowTowerNum;
                midY /= arrowTowerNum;
                pair<int,int> target = legalPlaceAround(114514, midX, midY);
                if(target.first == -1) {
                    DebugText("No legal place for priest");
                    return;
                }
                nextX = target.first;
                nextY = target.second;
                HumanMove(priestSN, target.first * BLOCKSIDELENGTH, target.second * BLOCKSIDELENGTH);
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

    //DebugText("priestX = " + to_string(priestX) + ", priestY = " + to_string(priestY));
    //DebugText("centerX = " + to_string(centerX) + ", centerY = " + to_string(centerY));
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
            //DebugText("i = " + to_string(i) + ", j = " + to_string(j) + "expV = " + to_string(totalValue));
            candPoints.push(make_tuple(totalValue, i, j));
        }
    }

    if(candPoints.empty()) return;
    int bestX = get<1>(candPoints.top());
    int bestY = get<2>(candPoints.top());

    //DebugText("bestX = " + to_string(bestX) + ", bestY = " + to_string(bestY) + ", expV = " + to_string(get<0>(candPoints.top())) + ", map = " + to_string(gameMap[bestX][bestY]));
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
    //DebugText("cx = " + to_string(cx) + ", cy = " + to_string(cy));
    //DebugText("buildingType = " + to_string(buildingType));
    int size = getBuildingSideLen(buildingType);
    for (int radius = 0; radius <= 8 ; ++radius) {
        for (int dx = -radius; dx <= radius; ++dx) {
            for (int dy = -radius; dy <= radius; ++dy) {
                int nx = cx + dx;
                int ny = cy + dy;
                if (nx >= 0 && nx + size <= MAP_SIZE && ny >= 0 && ny + size <= MAP_SIZE) {
                    int isLegal = true;
                    for(int i=-1;i<=size && isLegal;i++) {
                        for(int j=-1;j<=size;j++) {
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
    //static int beginningKit = 0;
    static int lastOrderId = -1;
    static pair<int,int> lastTarget;

    if(lastOrderId != -1) {
        auto it = info.ins_ret.find(lastOrderId);
        if(it != info.ins_ret.end() && it->second == ACTION_SUCCESS) {
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

    /*
    if(!beginningKit) {
        beginningKit = 1;
        buildTasks.push_back({BUILDING_HOME, homeX, homeY, {}});
    }*/

    tagFarmer builder;
    for(auto& farmer: info.farmers) {
        if(farmer.SN == builderSN) {
            builder = farmer;
            break;
        }
    }
    if(!buildTasks.empty() && builder.NowState == 0) {
        auto task = buildTasks.front();
        if(task.BlockDR > MAP_SIZE) {
            return;
        } else {
            if(checkResource(task.Type)) {
                pair<int,int> target = legalPlaceAround(task.Type, task.BlockDR, task.BlockUR);
                if(target.first == -1) {
                    DebugText("No legal place for target building");
                    return;
                }
                lastOrderId = HumanBuild(builder.SN, task.Type, target.first, target.second);
                lastTarget = target;
            }
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
            int nextResSN;
            int maxCnt = -1;
            for(auto& resource : info.resources) {
                if(resource.Type == RESOURCE_GAZELLE && resource.Blood <= 0 && resource.Cnt > maxCnt) {
                    //DebugText("resource.SN = " + to_string(resource.SN));
                    //DebugText("resource.Cnt = " + to_string(resource.Cnt));
                    maxCnt = resource.Cnt;
                    nextResSN = resource.SN;
                }
            }
            if(nextResSN == -1) return;
            for(auto& farmer : info.farmers) {
                if(hunterFarmersSN.count(farmer.SN) == 0) continue;  // 只派猎人收肉
                if(farmer.NowState == 0 && farmerState.count(farmer.SN) != 0 && farmerState[farmer.SN] == 0 
                    || farmer.NowState == 1 && farmerState.count(farmer.SN) != 0 && farmerState[farmer.SN] == 1) {
                    HumanAction(farmer.SN, nextResSN);
                }
                if(currentFarmersSN.find(farmer.SN) == currentFarmersSN.end()) {
                    currentFarmersSN.insert(farmer.SN);       
                    HumanAction(farmer.SN, nextResSN);
                }
                farmerState[farmer.SN] = farmer.NowState;
            }
            lastUpdateFrame = info.GameFrame;
        }
        bool allCollected = true;
        for(auto& resource: info.resources) {
            if(resource.Type == RESOURCE_GAZELLE && resource.Blood <= 0) {
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
            DebugText("stockX = " + to_string(stockX));
            DebugText("stockY = " + to_string(stockY));
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
                const int MAX_T = R - 2;         // 两塔最大切比雪夫偏移
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
    tagResource nearestGazelle;
    if(nearestGazelleSN == -1) {//旧猎物死亡，需要指定新的猎物
        nearestGazelleSN = getNearestResource(RESOURCE_GAZELLE, centerX, centerY, 0);
    }

    // 如果猎物还未死亡，初始化 nearestGazelle
    if(nearestGazelleSN != -1) {
        bool insight = false;
        for(auto& resource : info.resources) { 
            if(resource.SN == nearestGazelleSN) {
                nearestGazelle = resource;
                insight = true;
                break;
            }
        }
        if(!insight) return; //如果目标不在视野，等待重新进入视野
    }

    /*
    DebugText("GazelleSN = " + to_string(nearestGazelleSN));
    DebugText("GazelleBlood = " + to_string(nearestGazelle.Blood));
    DebugText("GazelleDR = " + to_string(nearestGazelle.DR));
    DebugText("GazelleUR = " + to_string(nearestGazelle.UR));
    DebugText("lastkillX = " + to_string(lastkillX));
    DebugText("lastkillY = " + to_string(lastkillY));
    DebugText("distance = " + to_string(euclidean_distance(nearestGazelle.DR, nearestGazelle.UR, lastkillX, lastkillY)));
    */
    
    //收集阶段切换逻辑：
    //首先需要击杀数量达到农民数一半
    //其次旧猎物死亡且视野内没有其它活猎物，或最近目标距离过远
    if(deadMeatResources.size() > info.farmers.size() / 2 &&
        (nearestGazelleSN == -1 || euclidean_distance(nearestGazelle.DR, nearestGazelle.UR, lastkillX, lastkillY) > 8 * BLOCKSIDELENGTH)) {
        huntingPhase = 1; //切换到收集阶段
        return;
    }
    

    for(auto& farmer: info.farmers) {
        if(hunterFarmersSN.count(farmer.SN) == 0) continue;  // 只派猎人，浆果/采石/维修村民不参与狩猎
        if(farmerTask[farmer.SN] != nearestGazelleSN) {
            farmerTask[farmer.SN] = nearestGazelleSN;
            HumanAction(farmer.SN, nearestGazelleSN);
        }
    }

    if(nearestGazelle.Blood <= 0) {
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

void UsrAI::logging() {
    static map<int,int> farmerState;         // 农民SN -> 上帧状态
    static map<int,int> farmerTreeAssignment; // 农民SN -> 树SN（保证每人一棵独占树）
    static set<int> occupiedTreeSNs;        // 当前被占用（含已砍完未释放）的树 SN
    static int lastOrderFrame = -1;
    const int orderInterval = 30;

    // 每次分配新树时实时搜索：距离仓库/市镇中心最近的、符合 isSingle、未被任何村民占用的树
    // 树砍完后从占用集合释放（资源列表中消失即视为砍完）
    {
        // 清理已消失（砍完）的树：占用集合和分配表中都不存在的资源
        set<int> liveTreeSNs;
        for(auto& r : info.resources)
            if(r.Type == RESOURCE_TREE) liveTreeSNs.insert(r.SN);
        for(auto it = occupiedTreeSNs.begin(); it != occupiedTreeSNs.end();) {
            if(liveTreeSNs.count(*it) == 0) it = occupiedTreeSNs.erase(it);
            else ++it;
        }
        for(auto it = farmerTreeAssignment.begin(); it != farmerTreeAssignment.end();) {
            if(liveTreeSNs.count(it->second) == 0) it = farmerTreeAssignment.erase(it);
            else ++it;
        }
    }

    auto findNearestFreeTree = [&]() -> int {
        int bestSN = -1, bestDist = INT_MAX;
        for(auto& resource : info.resources) {
            if(resource.Type != RESOURCE_TREE) continue;
            if(occupiedTreeSNs.count(resource.SN)) continue;   // 已被占用
            if(!isSingle(resource.BlockDR, resource.BlockUR)) continue;
            int minDist = INT_MAX;
            for(auto& building : info.buildings) {
                if(building.Type == BUILDING_CENTER || building.Type == BUILDING_STOCK) {
                    minDist = min(minDist, manhattan_distance(building.BlockDR, building.BlockUR,
                                                               resource.BlockDR, resource.BlockUR));
                }
            }
            if(minDist == INT_MAX) continue;   // 地图上无仓库/中心（理论不可达）
            if(minDist < bestDist) { bestDist = minDist; bestSN = resource.SN; }
        }
        return bestSN;
    };

    // gamePhase2 统一分配后：伐木工只从 treeFarmersSN 中产生（含卡脚重派），
    // 保障房屋/军队/科技的木材供应
    if(unifiedAssignDone) {
        if(info.GameFrame - lastOrderFrame <= orderInterval) return;
        lastOrderFrame = info.GameFrame;

        for(auto& farmer : info.farmers) {
            if(treeFarmersSN.count(farmer.SN) == 0) continue;    // 只认伐木组
            // 新人（无分配）或卡脚（连续两帧空闲）时分配/重派最近的空闲独占树
            if(farmerTreeAssignment.find(farmer.SN) == farmerTreeAssignment.end()
               || (farmerState[farmer.SN] == HUMAN_STATE_IDLE && farmer.NowState == HUMAN_STATE_IDLE)) {
                int treeSN = findNearestFreeTree();
                if(treeSN == -1) break;   // 无可分配的树
                // 重派时释放旧树
                auto it = farmerTreeAssignment.find(farmer.SN);
                if(it != farmerTreeAssignment.end()) occupiedTreeSNs.erase(it->second);
                farmerTreeAssignment[farmer.SN] = treeSN;
                occupiedTreeSNs.insert(treeSN);
                HumanAction(farmer.SN, treeSN);
            }
            farmerState[farmer.SN] = farmer.NowState;
        }
        return;
    }

    // gamePhase1 逻辑：未标记村民由伐木认领（同一棵树独占机制，与 gamePhase2 分支一致）
    if(info.GameFrame - lastOrderFrame <= orderInterval) return;
    lastOrderFrame = info.GameFrame;

    for(auto& farmer : info.farmers) {
        if(farmer.SN == builderSN) continue;
        // 跳过采石/维修/浆果/猎人/采金村民，避免伐木抢走已分配的采集任务
        if(stoneFarmersSN.count(farmer.SN) || repairFarmersSN.count(farmer.SN)
           || berryFarmersSN.count(farmer.SN) || hunterFarmersSN.count(farmer.SN)
           || goldFarmersSN.count(farmer.SN)) continue;
        // 新人（无分配）或卡脚（连续两帧空闲）时分配/重派最近的空闲独占树
        if(farmerTreeAssignment.find(farmer.SN) == farmerTreeAssignment.end()
           || (farmerState[farmer.SN] == HUMAN_STATE_IDLE && farmer.NowState == HUMAN_STATE_IDLE)) {
            int treeSN = findNearestFreeTree();
            if(treeSN == -1) break;   // 无可分配的树
            auto it = farmerTreeAssignment.find(farmer.SN);
            if(it != farmerTreeAssignment.end()) occupiedTreeSNs.erase(it->second);
            farmerTreeAssignment[farmer.SN] = treeSN;
            occupiedTreeSNs.insert(treeSN);
            HumanAction(farmer.SN, treeSN);
        }
        farmerState[farmer.SN] = farmer.NowState;
    }
}

void UsrAI::gamePhase1() {
    static bool buildingScheduled = false;
    static bool homeBuilt = false;
    if(info.Meat < 900){
        if(!homeBuilt) {
            homeBuilt = true;
            buildTasks.push_back({BUILDING_HOME, homeX, homeY, {}});
            DebugText("[homeDBG] phase1 initial home pushed @(" + to_string(homeX) + "," + to_string(homeY)
                      + ") frame=" + to_string(info.GameFrame));
        }
        // 需求2/3：新生成的村民统一分配至浆果采集（一对一）
        if(!resourceSwitchDone) berryCollecting();
        if(huntingPhase == 0) 
            hunting();
        else
            collecting();
    } else {
        if(!buildingScheduled) {
            buildTasks.push_back({BUILDING_ARMYCAMP, centerX, centerY, {}});
            buildTasks.push_back({BUILDING_MARKET, centerX, centerY, {}});
            buildTasks.push_back({BUILDING_STABLE, centerX, centerY, {}});
            buildTasks.push_back({BUILDING_RANGE, centerX, centerY, {}});
            buildTasks.push_back({BUILDING_COLLAGE, arrowTowerX, arrowTowerY, {}});
            //buildTasks.push_back({BUILDING_MARKET_GOLD_UPGRADE, marketSN, marketSN, {}});
            buildingScheduled = true;
        }

        // fixingArrowTower 已在 processData 统一调用，此处不再重复
        // 切换后保留的浆果/羚羊农民继续采集食物，采完空闲后由 logging() 统一收编伐木
        if(resourceSwitchDone) {
            berryCollecting();                 // 只维护保留的浆果农民，不再新增采集者
            if(huntingPhase == 0) hunting(); else collecting();
            // 羚羊/肉采集完毕（无活羚羊）时，释放猎人，交回 logging() 统一分配
            bool liveGazelle = false;
            for(auto& r : info.resources)
                if(r.Type == RESOURCE_GAZELLE && r.Cnt > 0) { liveGazelle = true; break; }
            if(!liveGazelle && deadMeatResources.empty() && huntingPhase == 0)
                hunterFarmersSN.clear();
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

    DebugText("nextFarmX = " + to_string(nextFarmX) + ", nextFarmY = " + to_string(nextFarmY));
    // 每建一块农田，从伐木组临时抽调一人；建完田该村民自动开始耕作（内核建完即驻田工作）
    if(!treeFarmersSN.empty()) {
        int sn = *treeFarmersSN.begin();
        treeFarmersSN.erase(sn);              // 移出伐木组，避免 logging() 再派他去伐木
        currentFarmersSN.insert(sn);
        farmerTask[sn] = BUILDING_FARM;
        HumanBuild(sn, BUILDING_FARM, nextFarmX, nextFarmY);
        farmNum++;
        return;
    }

    if(info.Wood < BUILD_FARM_WOOD || farmNum >= FARM_SLOTS) return;
    nextFarmPosition = legalPlaceAround(BUILDING_FARM, granaryX + farmDx[farmNum], granaryY + farmDy[farmNum]);
    nextFarmX = nextFarmPosition.first;
    nextFarmY = nextFarmPosition.second;
    if(nextFarmX == -1) return;

    // 第二块同帧农田：同样从伐木组抽调
    if(!treeFarmersSN.empty()) {
        int sn = *treeFarmersSN.begin();
        treeFarmersSN.erase(sn);
        HumanBuild(sn, BUILDING_FARM, nextFarmX, nextFarmY);
        farmerTask[sn] = 2;
        farmNum++;
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
    // 此时浆果/猎人组已由 resourceSwitch 释放（resourceSwitchDone 时两组为空），
    // 因此未被 berry/hunter/stone/repair 标记的村民即当前伐木工（treeFarmer）
    for(auto& farmer : info.farmers) {
        if(farmer.SN == builderSN) continue;
        if(berryFarmersSN.count(farmer.SN) || hunterFarmersSN.count(farmer.SN)) continue;
        if(stoneFarmersSN.count(farmer.SN) || repairFarmersSN.count(farmer.SN)) continue;
        treeFarmersSN.insert(farmer.SN);   // 保留伐木组，不删
    }

    // 采石组 → 全部归入采金组（清空原工种标记，stoneMining 之后自然空转）
    for(int sn : stoneFarmersSN)  goldFarmersSN.insert(sn);
    stoneFarmersSN.clear();
    // 维修组保留为常设工种（repairFarmersSN 不清空）：敌人在视野内时随时可修复箭塔；
    // 空闲期（无敌人且无可修箭塔）由 gamePhase2 临时借调去挖金（SN 进出 goldFarmersSN）
}

void UsrAI::goldMining() {
    static int first = 1;
    static int curIndex = 0;
    static GoldCluster bestCluster;
    if(first) {
        bestCluster = findBestGoldCluster();
        if(bestCluster.sns.empty()) return;
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
    static int hopliteNum = 0;
    const int maxHopliteNum = 5;
    for(auto& building : info.buildings) {
        if(building.Type == BUILDING_RANGE && building.Project == ACT_NULL && info.Human_Num < info.Human_MaxNum) {
            // 第二阶段：只训练复合弓兵（40食物+20黄金），不造战车弓兵
            if(compositeBowDone && info.Meat >= BUILDING_RANGE_CREATE_COMPOSITE_BOWMAN_FOOD
               && info.Gold >= BUILDING_RANGE_CREATE_COMPOSITE_BOWMAN_GOLD) {
                BuildingAction(building.SN, BUILDING_RANGE_CREATE_COMPOSITE_BOWMAN);
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
}

void UsrAI::Defense() {
    for(auto& army : info.armies) {
        if(army.SN == priestSN || army.NowState != HUMAN_STATE_IDLE) continue;
        vector<pair<int,int> > candEnemies;
        for(auto& enemy : info.enemy_armies) {
            if(arrowTowerSN.find(enemy.WorkObjectSN) != arrowTowerSN.end()) {
                int distance = manhattan_distance(enemy.BlockDR, enemy.BlockUR, army.BlockDR, army.BlockUR);
                candEnemies.push_back(make_pair(distance, enemy.SN));
            }
        }
        if(candEnemies.empty()) return;
        sort(candEnemies.begin(), candEnemies.end());
        HumanAction(army.SN, candEnemies[0].second);
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
            DebugText("[repairDBG] fixingArrowTower skipped: stone=" + to_string(info.Stone) + " < 50");
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
            DebugText("[repairDBG] fix order: farmer=" + to_string(farmer.SN)
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
        static bool wheelDone = false;

        // 上一条指令结果（ins_ret 仅下一帧有效），必须在节流判断前读取，读到即复位
        if(!wheelDone && wheelOrderId != -1) {
            auto it = info.ins_ret.find(wheelOrderId);
            if(it != info.ins_ret.end()) {
                DebugText("Wheel upgrade ins_ret code = " + to_string(it->second)
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
                DebugText("CompositeBow upgrade ins_ret code = " + to_string(it->second)
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
        bool liveGazelle = false;
        for(auto& r : info.resources)
            if(r.Type == RESOURCE_GAZELLE && r.Cnt > 0) { liveGazelle = true; break; }
        // 羚羊/尸体 全部枯竭后释放猎人，交回伐木；浆果农民由 berryCollecting 自行逐步释放
        if(!liveGazelle && deadMeatResources.empty() && huntingPhase == 0)
            hunterFarmersSN.clear();
    }

    // ==== 统一分配（仅一次）====：farming 组从伐木工中抽取；repair/stone 组全部归入采金组
    unifiedAssign();

    // 新生成的村民交替分配：第偶数个去砍树，第奇数个去采金（未被任何工种标记且非 builder 的在册村民）
    static int newFarmerIdx = 0;
    for(auto& farmer : info.farmers) {
        if(farmer.SN == builderSN) continue;
        if(berryFarmersSN.count(farmer.SN) || hunterFarmersSN.count(farmer.SN)) continue;
        if(stoneFarmersSN.count(farmer.SN) || repairFarmersSN.count(farmer.SN)
           || goldFarmersSN.count(farmer.SN)) continue;
        if(treeFarmersSN.count(farmer.SN) || currentFarmersSN.count(farmer.SN)) continue;  // 伐木/耕作组不得吸走
        if(newFarmerIdx % 2 == 0) {
            treeFarmersSN.insert(farmer.SN);   // 偶数序 → 伐木，logging() 会统一下发指令
            DebugText("[assignDBG] new farmer " + to_string(farmer.SN) + " -> TREE #" + to_string(newFarmerIdx));
        } else {
            goldFarmersSN.insert(farmer.SN);   // 奇数序 → 挖金，goldMining() 会统一下发指令
            DebugText("[assignDBG] new farmer " + to_string(farmer.SN) + " -> GOLD #" + to_string(newFarmerIdx));
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
            DebugText("[repairDBG] frame=" + to_string(info.GameFrame)
                      + " repairSN=" + to_string(repairFarmersSN.size())
                      + " borrowedGold=" + to_string(borrowed)
                      + " enemy=" + to_string(info.enemy_armies.size())
                      + " stone=" + to_string(info.Stone)
                      + " damagedTower=" + to_string(damagedTowers)
                      + " minBlood=" + (minTowerBlood == INT_MAX ? string("none") : to_string(minTowerBlood)));
            // 逐人状态（SN/是否在采金组/当前状态），确认指令是否真的下达到人
            for(auto& farmer : info.farmers) {
                if(repairFarmersSN.count(farmer.SN) == 0) continue;
                DebugText("[repairDBG] SN=" + to_string(farmer.SN)
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

    // 房屋上限 10 个：达上限后不再新建
    int homeCount = 0;
    for(auto& b : info.buildings)
        if(b.Type == BUILDING_HOME) homeCount++;
    // 调试：人口/上限/已建房屋数 + 未建满时推入任务
    static int dbgHomeFrame = -10000;
    if(info.GameFrame - dbgHomeFrame >= 50) {
        dbgHomeFrame = info.GameFrame;
        DebugText("[homeDBG] frame=" + to_string(info.GameFrame)
                  + " human=" + to_string(info.Human_Num) + "/" + to_string(info.Human_MaxNum)
                  + " homes=" + to_string(homeCount)
                  + " pending=" + to_string(buildTasks.size())
                  + (homeCount >= 10 ? " CAP_REACHED" : ""));
    }
    if(info.Human_Num >= info.Human_MaxNum - 2 && homeCount < 10) {
        // 去重：队列中已有待建房屋（含正在建造的）时不再推入，防止每帧重复 push
        bool homePending = false;
        for(auto& task : buildTasks)
            if(task.Type == BUILDING_HOME) { homePending = true; break; }
        if(!homePending) {
            buildTasks.push_back({BUILDING_HOME, homeX, homeY, {}});
            DebugText("[homeDBG] phase2 home pushed @(" + to_string(homeX) + "," + to_string(homeY)
                      + ") homes=" + to_string(homeCount + 1)
                      + " human=" + to_string(info.Human_Num) + "/" + to_string(info.Human_MaxNum)
                      + " frame=" + to_string(info.GameFrame));
        }
    }

    // 靶场优先生产复合弓兵（createArmy1）；食物不足以继续训练时才启动 farming 补食物
    if(info.Meat < BUILDING_RANGE_CREATE_COMPOSITE_BOWMAN_FOOD) farming();
    goldMining();     // 采金组（含原 repair/stone 组 + 新村民）
    createArmy1();
    logging();
}

bool UsrAI::checkArmy1() {
    const int chariotArcherNeed = 5;
    int chariotArcherNum = 0;
    for(auto& army: info.armies) {
        if(army.Sort == AT_CHARIOT_ARCHER) {
            chariotArcherNum++;
        }
    }
    if(chariotArcherNum >= chariotArcherNeed) return false;
    return true;
}

void UsrAI::gamePhase3() { // 全面反攻
    static int lastOrderFrame = -1;
    static pair<int,int> target = make_pair(-1,-1);
    if(info.GameFrame - lastOrderFrame < 30) return;
    lastOrderFrame = info.GameFrame;
    //聚集到同一点
    static bool gatherOrdered = false;
    if(!gatherOrdered) {
        target = legalPlaceAround(BUILDING_HOME, arrowTowerX, arrowTowerY);
        for(auto& army : info.armies) {
            if(army.NowState == HUMAN_STATE_WALKING) continue;
            HumanMove(army.SN, target.first, target.second);
        }
        gatherOrdered = true;
    }
    //检测是否到达目标点
    static bool allGathered = false;
    if(!allGathered) {
        for(auto& army : info.armies) {
            if(army.NowState == HUMAN_STATE_IDLE) continue;
            if(manhattan_distance(target.first, target.second, army.BlockDR, army.BlockUR) > 5) {
                return;
            }
        }
        allGathered = true;
    }
    //向大本营方向出发
    static bool departed = false;
    if(!departed) {
        int enemyCenterX = 100 - centerX, enemyCenterY = 100 - centerY;
        for(auto& army : info.armies) {
            HumanMove(army.SN, enemyCenterX, enemyCenterY);
        }
        departed = true;
    }

    static bool lastEnemyFound = false;
    bool enemyFound = info.enemy_armies.size() > 0;
    if(enemyFound) {
        lastEnemyFound = true;
        int enemyArrowTowerSN = -1;

        set<int> stoneThrowerSNs; int stoneThrowerTarget = -1;
        set<int> chariotArcherSNs; int chariotArcherTarget = -1;
        set<int> otherSNs; int otherTarget = -1;

        for(auto& army : info.armies) {
            if(army.Sort == AT_STONE_THROWER) {
                stoneThrowerSNs.insert(army.SN);
            } else if(army.Sort == AT_CHARIOT_ARCHER) {
                chariotArcherSNs.insert(army.SN);
            } else {
                otherSNs.insert(army.SN);
            }
        }

        for(auto& building : info.enemy_buildings) {
            if(building.Type == BUILDING_ARROWTOWER) {
                enemyArrowTowerSN = building.SN;
                break;
            }
        }

        if(enemyArrowTowerSN != -1) { // 敌方箭塔存在
            stoneThrowerTarget = enemyArrowTowerSN;
            otherTarget = enemyArrowTowerSN;
            for(auto& army : info.enemy_armies) {
                if(stoneThrowerSNs.find(army.WorkObjectSN) != stoneThrowerSNs.end()) {
                    chariotArcherTarget = army.SN;
                    break;
                }
            }
        } else { // 敌方箭塔不存在：终局目标——祭司贴邻转化敌方攻城武器厂（isWin 判胜）
            // 1.定位敌方攻城武器厂：必须已建成(Percent>=100)才能转化，且全程不可被误伤摧毁
            int enemySiegeSN = -1;
            for(auto& building : info.enemy_buildings) {
                if(building.Type == BUILDING_SIEGE && building.Blood > 0 && building.Percent >= 100) {
                    enemySiegeSN = building.SN;
                    break;
                }
            }

            // 2.守军威胁排序：正在攻击祭司的敌人 > 敌方投石车(10格AOE对我方集群致命) > 其他守军
            int priestAttackerSN = -1, enemyThrowerSN = -1, anyEnemySN = -1;
            for(auto& enemy : info.enemy_armies) {
                if(enemy.Blood <= 0) continue;
                if(priestAttackerSN == -1 && enemy.WorkObjectSN == priestSN) priestAttackerSN = enemy.SN;
                if(enemyThrowerSN == -1 && enemy.Sort == AT_STONE_THROWER) enemyThrowerSN = enemy.SN;
                if(anyEnemySN == -1) anyEnemySN = enemy.SN;
            }
            int guardTarget = (priestAttackerSN != -1) ? priestAttackerSN
                           : (enemyThrowerSN != -1 ? enemyThrowerSN : anyEnemySN);

            // 3.祭司走向攻城武器厂转化；仅在空闲时下令，避免打断已进行的 2-6 秒转化读条
            if(enemySiegeSN != -1 && priestSN != -1) {
                for(auto& army : info.armies) {
                    if(army.SN == priestSN && army.NowState == HUMAN_STATE_IDLE) {
                        HumanAction(priestSN, enemySiegeSN);
                        break;
                    }
                }
            }

            // 4.我方投石车：只与敌方投石车对射；严禁攻击建筑(AOE溅射会误伤待转化的攻城厂)，无敌方投石车则待命
            stoneThrowerTarget = enemyThrowerSN;

            // 5.战车弓兵+近战(方阵/阔剑)：优先清剿威胁祭司的敌人，其次近身强拆敌方投石车(其有最小射程盲区)，最后其他守军
            chariotArcherTarget = guardTarget;
            otherTarget = guardTarget;
        }

        if(stoneThrowerTarget != -1)
            for(auto& armySN : stoneThrowerSNs)  HumanAction(armySN, stoneThrowerTarget);
        if(chariotArcherTarget != -1)
            for(auto& armySN : chariotArcherSNs) HumanAction(armySN, chariotArcherTarget);
        if(otherTarget != -1)
            for(auto& armySN : otherSNs)         HumanAction(armySN, otherTarget);
        
    } else if(lastEnemyFound) {
        departed = false;
        lastEnemyFound = false;
    }
    
}

void UsrAI::assignFarmer()
{
    static int gamePhase1Over = 0;
    static int gamePhase2Over = 0;
    if(!gamePhase1Over && info.civilizationStage <= CIVILIZATION_TOOLAGE) {
        HumanControl = 16;
        gamePhase1();
    } else if(!gamePhase2Over && checkArmy1()) {
        gamePhase1Over = 1;
        HumanControl = 30;
        gamePhase2();
    } else {
        gamePhase2Over = 1;
        HumanControl = 50;
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
    //DebugText("current towerSN = " + to_string(towerSN));
    //static map<int, bool > haveAttacked;
    static map<int, int > lastAttackFrame;
    const int attackRange = 7;
    const int attackInterval = 40;

    if(info.GameFrame - lastAttackFrame[towerSN] < attackInterval) return;
    lastAttackFrame[towerSN] = info.GameFrame;

    set<int> candEnemySNs;
    int nextEnemySN = -1;

    for(auto& enemy: info.enemy_armies) { 
        if(abs(BlockDR - enemy.BlockDR) <= attackRange && abs(BlockUR - enemy.BlockUR) <= attackRange) {
            candEnemySNs.insert(enemy.SN);
            if(nextEnemySN == -1 && enemy.WorkObjectSN == priestSN) {
                nextEnemySN = enemy.SN;
            }
        }
    }
    
    if(nextEnemySN == -1) {
        double maxAtkHpRatio = 0;
        for(auto& enemy: info.enemy_armies) { //找出当前攻血比最高的敌人
            if(candEnemySNs.find(enemy.SN) == candEnemySNs.end() || enemy.Blood <= 0) continue;
            if((double)enemy.attack / enemy.Blood > maxAtkHpRatio) {
                maxAtkHpRatio = (double)enemy.attack / enemy.Blood;
                nextEnemySN = enemy.SN;
            }
        }
    }

    HumanAction(towerSN, nextEnemySN);
}

void UsrAI::assignArmy()
{
    for(auto& building: info.buildings) {
        if(building.Type == BUILDING_ARROWTOWER) {
            arrowTowerAttack(building.SN, building.BlockDR, building.BlockUR);
        }
    }

    // 优先级：正在攻击祭祀的敌人 > 与自身不同兵种(近战/远程)的敌人 > 其他敌人；同层级内取最近
    for(auto& army : info.armies) {
        if(army.SN == priestSN || army.NowState != HUMAN_STATE_IDLE) continue;
        bool armyIsMelee = (getAttackRange(army.Sort) <= 1);
        int targetSN = -1;
        int bestTier = 3;       // 1=攻击祭祀 2=不同兵种 3=其他
        int bestDist = INT_MAX;
        for(auto& enemy : info.enemy_armies) {
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

// 需求2/3：新生成的村民统一分配至浆果采集，单个村民与单个浆果一一对应
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
            // 所有浆果全部枯竭，释放该农民，交回 logging 收编
            berryFarmersSN.erase(farmer.SN);
        }
    }

    // 切换完成后不再新增浆果采集者（保留的农民持续采集至全部枯竭）
    if(resourceSwitchDone) return;

    for(auto& farmer : info.farmers) {
        if(farmer.SN == builderSN) continue;
        if(hunterFarmersSN.count(farmer.SN) || berryFarmersSN.count(farmer.SN)
           || stoneFarmersSN.count(farmer.SN) || repairFarmersSN.count(farmer.SN)) continue;

        int berrySN = -1;
        for(int sn : berryResourceSNs) {
            if(available.count(sn) && assignedBerrySNs.count(sn) == 0) { berrySN = sn; break; }
        }
        if(berrySN == -1) {
            // 所有浆果都已有村民采集，新村民加入打猎
            hunterFarmersSN.insert(farmer.SN);
            continue;
        }

        assignedBerrySNs.insert(berrySN);
        berryAssignment[farmer.SN] = berrySN;
        berryFarmersSN.insert(farmer.SN);
        HumanAction(farmer.SN, berrySN);
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

// 需求4：食物总量>=900 且 人口>=16 时，浆果农民前3人转采石，其余对半分(一半继续采浆果 / 一半转采羚羊)，原猎人交回伐木
void UsrAI::resourceSwitch() {
    if(resourceSwitchDone) return;
    if(info.Meat < 900 || info.Human_Num < 16) return;
    resourceSwitchDone = true;

    // 原猎人 → 伐木：清空猎人标记，交回 logging() 统一认领（meat>=900 分支会调用 logging）
    hunterFarmersSN.clear();

    // 浆果农民改造:前3人转采石，其余按序交替分配——一半继续采浆果，一半转入猎人继续采羚羊
    vector<int> berryList(berryFarmersSN.begin(), berryFarmersSN.end());   // 提前拷贝，避免遍历中修改集合
    const int STONE_PICK_NUM = 3;                                          // 转采石的浆果农民数量
    for(int idx = 0; idx < (int)berryList.size(); idx++) {
        int sn = berryList[idx];
        if(idx < STONE_PICK_NUM) {
            // 前3人转采石
            stoneFarmersSN.insert(sn);
            berryFarmersSN.erase(sn);
            berryAssignment.erase(sn);
        } else if((idx - STONE_PICK_NUM) % 2 == 0) {
            // 一半继续采浆果：保留在 berryFarmersSN，既有 berryAssignment 继续生效
        } else {
            // 一半转采羚羊：移入猎人集合，由 hunting()/collecting() 统一管理
            hunterFarmersSN.insert(sn);
            berryFarmersSN.erase(sn);
            berryAssignment.erase(sn);
        }
    }
    // 修理箭塔逻辑在 repairRepurpose()/fixingArrowTower() 中保持不变

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
            DebugText("[woodDBG] Wood upgrade ins_ret code = " + to_string(it->second)
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
            DebugText("[woodDBG] Wood upgrade ordered, frame = " + to_string(info.GameFrame));
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
    DebugText("[repairDBG] repairRepurpose @13500: repairFarmers={" + snList + "}");
}

void UsrAI::processData()
{
    init();

    updateTech();

    assignArmy();
    priest();

    // 需求4：资源动态切换（食物>=900 && 人口>=16 → 前3名浆果农民转采石，其余继续采浆果/羚羊，原猎人转伐木）
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
    assignBuilding();
    
    createFarmer();
}

//farming()未触发