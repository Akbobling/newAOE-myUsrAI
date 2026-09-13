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
bool hunterInitialized = false;    // 猎人组是否已初始化
bool resourceSwitchDone = false;   // 需求4：食物/人口达标后切采石+伐木
bool repairRepurposeDone = false;  // 需求5：14000帧采石村民二次分配


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
    int lastOrderTime = 0;
    if(info.GameFrame - lastOrderTime < 100) return;
    lastOrderTime = info.GameFrame;

    if(info.Meat < BUILDING_CENTER_UPGRADE_BRONZEAGE_FOOD) DebugText("Not enough meat to upgrade center");
    if(!checkRequiredBuilding()) DebugText("Not enough required buildings to upgrade center");
    if(info.civilizationStage <= CIVILIZATION_TOOLAGE && info.Meat >= BUILDING_CENTER_UPGRADE_BRONZEAGE_FOOD && checkRequiredBuilding()) {
        for(auto& building : info.buildings) {
            if(building.Type == BUILDING_CENTER) {
                lastOrderId = BuildingAction(building.SN, BUILDING_CENTER_UPGRADE);
                break;
            }
        }
    }

    // 通过 ins_ret 获取升级铜器时代指令的状态码
    if(lastOrderId != -1) {
        auto it = info.ins_ret.find(lastOrderId);
        if(it != info.ins_ret.end()) {
            DebugText("BronzeAge upgrade ins_ret code = " + to_string(it->second));
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

    //DebugText("lastOrderX = " + to_string(lastOrderX) + ", lastOrderY = " + to_string(lastOrderY));
    //DebugText("manhattan_distance = " + to_string(manhattan_distance(lastOrderX,lastOrderY,priestX,priestY)))
    if(info.GameFrame > 5500) {
        // 需求7：转化保护——转化中且未受攻击(Blood未减少)时，暂停移动逻辑
        if(converting) {
            bool targetExists = false;
            for(auto& enemy : info.enemy_armies) {
                if(enemy.SN == convertTargetSN) { targetExists = true; break; }
            }
            if(targetExists && priestBlood != -1 && lastPriestBlood != -1 && priestBlood >= lastPriestBlood) {
                return;  // 未受攻击且目标仍在：继续转化，不发移动指令
            }
            converting = false;  // 受到攻击或转化完成(目标消失)
        }
        lastPriestBlood = priestBlood;

        if(info.GameFrame - lastOrderFrame < retryInterval) {
            return;
        }
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
            // 需求7：不在任何仇恨敌人攻击范围内时，选择最近的敌方方阵兵进行转化
            int targetSN = -1;
            int minDist = INT_MAX;
            for(auto& enemy : info.enemy_armies) {
                if(enemy.Sort == AT_HOPLITE) {
                    int d = manhattan_distance(enemy.BlockDR, enemy.BlockUR, priestX, priestY);
                    if(d < minDist) { minDist = d; targetSN = enemy.SN; }
                }
            }
            if(targetSN != -1) {
                HumanAction(priestSN, targetSN);  // 转化（内核 ATTACKTYPE_CHANGE）
                converting = true;
                convertTargetSN = targetSN;
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
                pair<int,int> target = legalPlaceAround(BUILDING_HOME, midX, midY);
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
        if(checkResource(task.Type)) {
            if(task.BlockDR > MAP_SIZE) {
                BuildingAction(task.BlockDR, task.Type); //BUILDING_MARKET_WOOD_UPGRADE
            } else {
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
                if(farmer.SN != builderSN) {
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
                const int MAX_T = 2 * R;         // 两塔最大切比雪夫偏移
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

void UsrAI::logging() {
    static priority_queue<pair<int,int>, vector<pair<int,int>>, greater<pair<int,int>>> candTreeSNs;
    static map<int,int> farmerState;
    static int lastOrderFrame = -1;
    const double INF = 1e9;
    const int orderInterval = 50;
    if(candTreeSNs.empty()) {
        for(auto& resource : info.resources) {
            bool isSingle = treeCoverMap[resource.BlockDR][resource.BlockUR] <= 1;
            if(resource.Type == RESOURCE_TREE) {
                DebugText("X = " + to_string(resource.BlockDR) + ", Y = " + to_string(resource.BlockUR));
                DebugText("isSingle = " + to_string(isSingle));
            }
            if(resource.Type == RESOURCE_TREE && isSingle) {
                int minDistance = INF;
                for(auto& building : info.buildings) {
                    if(building.Type == BUILDING_CENTER || building.Type == BUILDING_STOCK) {
                        minDistance = min(minDistance, manhattan_distance(building.BlockDR, building.BlockUR, resource.BlockDR, resource.BlockUR));
                    }
                }
                if(minDistance != INF) {
                    candTreeSNs.push(make_pair(minDistance, resource.SN));
                }
                if(resource.BlockDR >= 25 && resource.BlockUR <= 27 && resource.BlockUR >= 77 && resource.BlockUR <= 79) {
                    DebugText("minDistance = " + to_string(minDistance));
                }
            }
        }
        if(candTreeSNs.empty()) return;
        for(auto& farmer : info.farmers) {
            if(candTreeSNs.empty()) break;   // 树少于农民数时停止，防止越界
            // 跳过采石/维修村民，避免伐木抢走已分配的采集任务
            if(farmer.SN != builderSN
               && stoneFarmersSN.count(farmer.SN) == 0
               && repairFarmersSN.count(farmer.SN) == 0) {
                farmerState[farmer.SN] = HUMAN_STATE_WORKING;
                HumanAction(farmer.SN, candTreeSNs.top().second);
                candTreeSNs.pop();
            }
        }
    }

    if(info.GameFrame - lastOrderFrame <= orderInterval) return;
    lastOrderFrame = info.GameFrame;

    for(auto& farmer : info.farmers) {
        if(candTreeSNs.empty()) break;       // 候选耗尽不再 pop
        if(farmer.SN == builderSN) continue;
        if(stoneFarmersSN.count(farmer.SN) || repairFarmersSN.count(farmer.SN)) continue;
        if(currentFarmersSN.find(farmer.SN) == currentFarmersSN.end() || farmerState[farmer.SN] == HUMAN_STATE_IDLE && farmer.NowState == HUMAN_STATE_IDLE) {
            DebugText("farmer" + to_string(farmer.SN)+" now is idle, assigning tree...");
            currentFarmersSN.insert(farmer.SN);
            HumanAction(farmer.SN, candTreeSNs.top().second);
            candTreeSNs.pop();
        }
        farmerState[farmer.SN] = farmer.NowState;
    }

}

void UsrAI::gamePhase1() {
    static bool buildingScheduled = false;
    static bool homeBuilt = false;
    if(info.Meat < 800){
        if(!homeBuilt) {
            homeBuilt = true;
            buildTasks.push_back({BUILDING_HOME, homeX, homeY, {}});
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
    for(auto& farmer: info.farmers) {
        if(currentFarmersSN.find(farmer.SN) == currentFarmersSN.end()) {
            currentFarmersSN.insert(farmer.SN);
            farmerTask[farmer.SN] = BUILDING_FARM;
            HumanBuild(farmer.SN, BUILDING_FARM, nextFarmX, nextFarmY);
            farmNum++;
            break;
        }
    }

    if(info.Wood < BUILD_FARM_WOOD || farmNum >= FARM_SLOTS) return;
    nextFarmPosition = legalPlaceAround(BUILDING_FARM, granaryX + farmDx[farmNum], granaryY + farmDy[farmNum]);
    nextFarmX = nextFarmPosition.first;
    nextFarmY = nextFarmPosition.second;
    if(nextFarmX == -1) return;

    for(auto& farmer: info.farmers) {
        if(farmer.SN != builderSN && farmerTask[farmer.SN] != 2) { //1表示logging 2表示farming
            HumanBuild(farmer.SN, BUILDING_FARM, nextFarmX, nextFarmY);
            farmerTask[farmer.SN] = 2;
            farmNum++;
            break;
        }
    }
}

void UsrAI::goldMining() {
    static int first = 1;
    static int curIndex = 0;
    static GoldCluster bestCluster;
    if(first) {
        bestCluster = findBestGoldCluster();
        if(bestCluster.sns.empty()) return;
        buildTasks.push_back({BUILDING_STOCK, bestCluster.centerX, bestCluster.centerY, {} });
        first = 0;
    }
    for(auto& farmer: info.farmers) {
        if(currentFarmersSN.find(farmer.SN) == currentFarmersSN.end()) {
            currentFarmersSN.insert(farmer.SN);
            farmerTask[farmer.SN] = 3; // 3表示mining
            HumanAction(farmer.SN, bestCluster.sns[curIndex++]);
            if(curIndex >= bestCluster.sns.size()) curIndex = 0;
        }
    }
}

void UsrAI::createArmy1() {
    static int hopliteNum = 0;
    const int maxHopliteNum = 5;
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
    // 维修者：builder（始终优先）+ repairFarmersSN（14000帧后从采石组二次分配而来）
    static bool flag = false;
    static size_t lastRepairCount = 0;   // 上帧维修组人数，用于检测新增维修者后重新强制分配
    int minBlood = INT_MAX;
    int targetSN = -1;

    if(info.Stone < 5) {
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

    // 首次分配立即切换；之后仅派空闲的维修者，不打断正在建造/采集的人
    for(auto& farmer : info.farmers) {
        bool isRepairer = (farmer.SN == builderSN) || repairFarmersSN.count(farmer.SN);
        if(!isRepairer) continue;
        if(!flag || farmer.NowState == HUMAN_STATE_IDLE) {
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
    const int maxFarmNum = 8;

    if(!farmerInitialised) {
        for(auto& farmer: info.farmers) {
            currentFarmersSN.insert(farmer.SN);
            if(farmer.SN != builderSN) {
                farmerTask[farmer.SN] = 1; // 1表示logging 2表示farming
            }
        }
        farmerInitialised = 1;
    }

    // fixingArrowTower 已在 processData 统一调用，此处不再重复，也不 return
    if(farmBuildingPhase){
        farming();
        if(farmNum >= maxFarmNum) 
            farmBuildingPhase = 0;
    } else if(miningPhase || buildTasks.empty()){
        goldMining();
        createArmy1();
        Defense();
        if(!miningPhase) {
            buildTasks.push_back({BUILDING_MARKET_GOLD_UPGRADE, marketSN, marketSN, {}});
            miningPhase = 1;
        }
    }

    logging();
}

bool UsrAI::checkArmy1() {
    return false;
}

void UsrAI::gamePhase3() {
    return;
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

    for(auto& farmer : info.farmers) {
        if(farmer.SN == builderSN) continue;
        if(hunterFarmersSN.count(farmer.SN) || berryFarmersSN.count(farmer.SN)
           || stoneFarmersSN.count(farmer.SN) || repairFarmersSN.count(farmer.SN)) continue;

        int berrySN = -1;
        for(int sn : berryResourceSNs) {
            if(available.count(sn) && assignedBerrySNs.count(sn) == 0) { berrySN = sn; break; }
        }
        if(berrySN == -1) break;  // 浆果不够，等待回收

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

// 需求4：食物总量>=800 且 人口>=16 时，浆果采集者转采石、猎人转伐木
void UsrAI::resourceSwitch() {
    if(resourceSwitchDone) return;
    if(info.Meat < 800 || info.Human_Num < 16) return;
    resourceSwitchDone = true;

    // 浆果采集村民 → 采石
    for(int sn : berryFarmersSN) {
        stoneFarmersSN.insert(sn);
    }
    berryFarmersSN.clear();
    berryAssignment.clear();

    // 猎人 → 伐木：清空猎人标记，交回 logging() 统一认领（meat>=800 分支会调用 logging）
    hunterFarmersSN.clear();
}

// 需求5：14000帧（第二波敌人进攻阶段）采石村民二次分配：4人继续采石，其余转箭塔维修
void UsrAI::repairRepurpose() {
    if(repairRepurposeDone) return;
    if(info.GameFrame < 14000) return;
    repairRepurposeDone = true;

    // 从采石村民中选前4人继续采石，其余转维修
    vector<int> stoneList(stoneFarmersSN.begin(), stoneFarmersSN.end());
    const int keepStoneNum = 4;
    for(int i = keepStoneNum; i < (int)stoneList.size(); i++) {
        stoneFarmersSN.erase(stoneList[i]);
        repairFarmersSN.insert(stoneList[i]);
    }
}

void UsrAI::processData()
{
    init();

    updateTech();

    assignArmy();
    priest();

    // 需求4：资源动态切换（食物>=800 && 人口>=16 → 浆果转采石 / 猎人转伐木）
    resourceSwitch();
    // 需求5：14000帧采石村民二次分配（4人采石 / 其余修塔）
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