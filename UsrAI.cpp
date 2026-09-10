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
int treeCoverMap[MAP_SIZE+1][MAP_SIZE+1];
vector<buildTask> buildTasks;
map<int, int> farmerTask;
int builderSN = -1;
int totalMeat = 200;
double lastkillX = -1, lastkillY = -1;
int granaryX = -1, granaryY = -1;
int nearestResourceX, nearestResourceY;
int needStock = 0;
int marketSN = -1;
bool isUpdatingStage = false;

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
                    if(info.resources[i].Type == RESOURCE_GAZELLE || info.resources[i].Type == RESOURCE_GOLD) {
                        exploreValueMap[nx][ny] += 1;
                    }
                    if(info.resources[i].Type == RESOURCE_LION) {
                        exploreValueMap[nx][ny] -= 1;
                    }
                    if(info.resources[i].Type == RESOURCE_TREE) {
                        treeCoverMap[nx][ny] += 1;
                        exploreValueMap[nx][ny] -= 2;
                    }
                }
            }
        }
    }

    // 遍历所有建筑并在map上标注为已占用
    for (int i = 0; i < info.buildings.size(); i++) {
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
    
    for (int i=0; i < info.farmers.size(); i++) {
        int fx = info.farmers[i].BlockDR;
        int fy = info.farmers[i].BlockUR;
        if (fx >= 0 && fx < MAP_SIZE && fy >= 0 && fy < MAP_SIZE) {
            gameMap[fx][fy] = 1;
        }
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
    if(info.civilizationStage <= CIVILIZATION_TOOLAGE && info.Meat >= BUILDING_CENTER_UPGRADE_BRONZEAGE_FOOD && checkRequiredBuilding()) {
        for(auto& building : info.buildings) {
            if(building.Type == BUILDING_CENTER) {
                BuildingAction(building.SN, BUILDING_CENTER_UPGRADE);
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
int getAttackRange(int armyType)
{
    switch (armyType) {
        case AT_BOWMAN:            return 5;    // 弓箭手
        case AT_COMPOSITE_BOWMAN:  return 7;    // 复合弓兵
        case AT_CHARIOT_ARCHER:    return 7;    // 战车弓兵
        case AT_STONE_THROWER:     return 10;   // 投石车
        case AT_SLINGER:           return 4;    // 投石兵
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
    const int retryInterval = 50;
    const int radius = 5;

    int priestX = -1, priestY = -1;
    for(auto& army: info.armies) {
        if(army.Sort == AT_PRIEST) {
            priestX = army.BlockDR;
            priestY = army.BlockUR;
            break;
        }
    }

    //DebugText("lastOrderX = " + to_string(lastOrderX) + ", lastOrderY = " + to_string(lastOrderY));
    //DebugText("manhattan_distance = " + to_string(manhattan_distance(lastOrderX,lastOrderY,priestX,priestY)))
    if(info.GameFrame > 5500) {
        if(info.GameFrame - lastOrderFrame < retryInterval) {
            return;
        }
        int nextX = -1, nextY = -1;
        bool ordered = false;
        for(auto& enemy: info.enemy_armies) {
            if(enemy.WorkObjectSN == priestSN && manhattan_distance(enemy.BlockDR,enemy.BlockUR,priestX,priestY) <= getAttackRange(enemy.Sort) + 1) {
                int dirX = (priestX > enemy.BlockDR) ? 1 : (priestX < enemy.BlockDR ? -1 : 0);
                int dirY = (priestY > enemy.BlockUR) ? 1 : (priestY < enemy.BlockUR ? -1 : 0);
                int fleeX = priestX, fleeY = priestY;
                for(int step = 1; step <= 15; step++) {
                    int nx = priestX + dirX * step;
                    int ny = priestY + dirY * step;
                    if(nx < 0 || nx >= MAP_SIZE || ny < 0 || ny >= MAP_SIZE) break;
                    if(gameMap[nx][ny] == 1) break; 
                    fleeX = nx; 
                    fleeY = ny;
                }
                nextX = fleeX;
                nextY = fleeY;
                HumanMove(priestSN, fleeX * BLOCKSIDELENGTH, fleeY * BLOCKSIDELENGTH);
                ordered = true;
                break;
            }
        }
        if(!ordered) {
            pair<int,int> target = legalPlaceAround(BUILDING_HOME, arrowTowerX, arrowTowerY);
            if(target.first == -1) {
                DebugText("No legal place for priest");
                return;
            }
            nextX = target.first;
            nextY = target.second;
            DebugText("priest to X = " + to_string(target.first) + ", priest to Y = " + to_string(target.second) + "map[" + to_string(target.first) + "][" + to_string(target.second) + "]" + to_string(gameMap[target.first][target.second]));
            HumanMove(priestSN, target.first * BLOCKSIDELENGTH, target.second * BLOCKSIDELENGTH);
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
            if(manhattan_distance(i,j,centerX,centerY) > 60 || gameMap[i][j] == -1 || gameMap[i][j] == 1) continue;
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
    DebugText("cx = " + to_string(cx) + ", cy = " + to_string(cy));
    DebugText("buildingType = " + to_string(buildingType));
    int size = getBuildingSideLen(buildingType);
    for (int radius = 0; radius <= 8 ; ++radius) {
        for (int dx = -radius; dx <= radius; ++dx) {
            for (int dy = -radius; dy <= radius; ++dy) {
                int nx = cx + dx;
                int ny = cy + dy;
                if (nx >= 0 && nx + size <= MAP_SIZE && ny >= 0 && ny + size <= MAP_SIZE) {
                    int isLegal = true;
                    for(int i=0;i<size && isLegal;i++) {
                        for(int j=0;j<size;j++) {
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
                if(building.BlockDR == lastTarget.first && building.BlockUR == lastTarget.second && building.Percent < 100) {
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
    static map<int,int> farmerState;
    static int lastUpdateFrame = -1;
    static int bushHuntersNum = 0;
    static int buildingScheduled = 0;
    const int maxBushHuntersNum = 2;
    if(deadMeatResources.empty()) {
        if(info.GameFrame - lastUpdateFrame > 25) {
            int nextResSN = getNearestResource(RESOURCE_BUSH, centerX, centerY, -1);
            if(nextResSN == -1) DebugText("No valid bush resource");
            if(nextResSN == -1 || bushHuntersNum >= maxBushHuntersNum) {
                int maxCnt = -1;
                for(auto& resource : info.resources) {
                    if(resource.Type == RESOURCE_GAZELLE && resource.Blood <= 0 && resource.Cnt > maxCnt) {
                        DebugText("resource.SN = " + to_string(resource.SN));
                        DebugText("resource.Cnt = " + to_string(resource.Cnt));
                        maxCnt = resource.Cnt;
                        nextResSN = resource.SN;
                    }
                }
            }
            if(nextResSN == -1) return;
            for(auto& farmer : info.farmers) {
                if(farmer.SN == builderSN) continue;
                if(farmer.NowState == 0 && farmerState.count(farmer.SN) != 0 && farmerState[farmer.SN] == 0 
                    || farmer.NowState == 1 && farmerState.count(farmer.SN) != 0 && farmerState[farmer.SN] == 1) {
                    HumanAction(farmer.SN, nextResSN);
                    bushHuntersNum ++;
                }
                if(currentFarmersSN.find(farmer.SN) == currentFarmersSN.end()) {
                    currentFarmersSN.insert(farmer.SN);       
                    HumanAction(farmer.SN, nextResSN);
                    bushHuntersNum ++;
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
            bushHuntersNum = 0;
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
            buildTasks.push_back({BUILDING_ARROWTOWER, centerX, centerY, {}});
            buildingScheduled = 1;
        }
        if(!stockBuilt) return;

        nearestGazelleSN = -1;
        int i = 0;
        for(auto& farmer: info.farmers) {
            if(farmer.SN != builderSN) {
                farmerTask[farmer.SN] = deadMeatResources[i];
                HumanAction(farmer.SN, deadMeatResources[i]); 
                i++;
                if(i >= deadMeatResources.size()) i = 0;
            }
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
        if(farmer.SN != builderSN) {
            if(farmerTask[farmer.SN] != nearestGazelleSN) {
                farmerTask[farmer.SN] = nearestGazelleSN;
                HumanAction(farmer.SN, nearestGazelleSN);
            }
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
            if(farmer.SN != builderSN) {
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
    if(info.Meat < 800){
        if(huntingPhase == 0) 
            hunting();
        else
            collecting();
    } else {
        if(!buildingScheduled) {
            buildTasks.push_back({BUILDING_ARMYCAMP, arrowTowerX, arrowTowerY, {}});
            buildTasks.push_back({BUILDING_MARKET, arrowTowerX, arrowTowerY, {}});
            buildTasks.push_back({BUILDING_STABLE, arrowTowerX, arrowTowerY, {}});
            buildTasks.push_back({BUILDING_RANGE, arrowTowerX, arrowTowerY, {}});
            buildTasks.push_back({BUILDING_COLLAGE, arrowTowerX, arrowTowerY, {}});
            //buildTasks.push_back({BUILDING_MARKET_GOLD_UPGRADE, marketSN, marketSN, {}});
            buildingScheduled = true;
        }
        assignArmy();
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
        HumanControl = 10;
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
            return CIVILIZATION_STONEAGE;
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
    static map<int, bool > haveAttacked;
    static map<int, int > lastAttackFrame;
    const int attackRange = 7;
    const int attackInterval = 60;

    if(info.GameFrame - lastAttackFrame[towerSN] < attackInterval) return;
    lastAttackFrame[towerSN] = info.GameFrame;

    set<int> candEnemySNs;
    int nextEnemySN = -1;

    for(auto& enemy: info.enemy_armies) { //加入新敌人
        if(manhattan_distance(BlockDR, BlockUR, enemy.BlockDR, enemy.BlockUR) > attackRange) continue;
        candEnemySNs.insert(enemy.SN);
    }

    for(auto& enemySN: candEnemySNs) { //优先攻击新敌人
        if(haveAttacked.count(enemySN) == 0) {
            haveAttacked[enemySN] = false;
        }
        if(!haveAttacked[enemySN]) {
            haveAttacked[enemySN] = true;
            DebugText("tower " + to_string(enemySN)+" is attacking enemy " + to_string(enemySN));
            nextEnemySN = enemySN;
            break;
        }
    }
    
    if(nextEnemySN != -1) {
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
}

void UsrAI::processData()
{
    init();

    updateTech();        

    priest();
    assignFarmer();
    assignBuilding();
    
    createFarmer();
}

//farming()未触发