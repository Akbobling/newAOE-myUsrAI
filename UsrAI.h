#ifndef USRAI_H
#define USRAI_H

#include "ai.h"
#include <unordered_map>

extern tagGame tagUsrGame;
extern ins UsrIns;
/*##########DO NOT MODIFY THE CODE ABOVE##########*/

struct GoldCluster {
    int centerX;       // 簇质心（块坐标）
    int centerY;
    vector<int> sns;   // 簇内金矿资源点 SN
};

class UsrAI:public AI
{
public:
    UsrAI(){this->id=0;}
    ~UsrAI(){}

private:
    void processData() override;
    tagInfo getInfo(){return tagUsrGame.getInfo();}
    int AddToIns(instruction ins) override
    {
        UsrIns.lock.lock();
        ins.id=UsrIns.g_id;
        UsrIns.g_id++;
        UsrIns.instructions.push(ins);
        UsrIns.lock.unlock();
        return ins.id;
    }
    void clearInsRet() override
    {
        tagUsrGame.clearInsRet();
    }
    /*##########DO NOT MODIFY THE CODE IN THE CLASS##########*/
    void collectResource();
    void arrowTowerAttack(int towerSN, int BlockDR, int BlockUR);
    void getLegalPlace(tagObj, int, int);
    /*##########YOUR CODE BEGINS HERE##########*/
    int getBuildingSideLen(int type);
    int getResourceSideLen(int type);
    void init();
    void updateStage();
    void updateTech();  
    void createFarmer();
    void createArmy();
    void seek();
    pair<int,int> legalPlaceAround(int buildingType, int cx, int cy);
    void assignBuilding();
    double euclidean_distance(double x1, double y1, double x2, double y2);
    int manhattan_distance(int x1, int y1, int x2, int y2);
    int getNearestResource(int resourceType, int cx, int cy, int aliveOnly);
    GoldCluster findBestGoldCluster();
    void assignFarmer();
    int getBuildingRequiredAge(int buildingType);
    int getBuildingWoodCost(int buildingType);
    int getBuildingStoneCost(int buildingType);
    bool checkResource(int buildingType);
    bool checkRequiredBuilding();
    void hunting();
    void collecting();
    void assignArmy();
    void priest();
    void logging();
    void farming();
    void goldMining();
    void createArmy1();
    bool checkArmy1();
    void Defense();
    void gamePhase1();
    void gamePhase2();
    void gamePhase3();
    /*##########YOUR CODE ENDS HERE##########*/


};

struct buildTask {
    int Type;   // 建筑类型
    int BlockDR;       // 当前尝试点（左上角块坐标 DR）
    int BlockUR;       // 当前尝试点（左上角块坐标 UR）
    vector<int> builderSNs;   // 建筑工 SN 列表
};

/*##########YOUR CODE BEGINS HERE##########*/

bool compareSN(tagObj, tagObj);
bool compareDistance(tagObj, tagObj);
bool compareBlood(tagHuman, tagHuman);
double getDistance(tagObj, tagObj);
void getResource();
void getOccupied();
tagBuilding getLastBuilding(int);
void occupied(tagObj, int);



/*##########YOUR CODE ENDS HERE##########*/
#endif // USRAI_H
