#ifndef __SCENE_PATH_MAP_H__
#define __SCENE_PATH_MAP_H__

// 最大场景尺寸
const int MAX_SCENE_X = 1000;
const int MAX_SCENE_Y = 1000;
const int MIN_SCENE_BLOCK_SIZE = 1;
const int MAX_SCENE_BLOCK_X = (MAX_SCENE_X / MIN_SCENE_BLOCK_SIZE);
const int MAX_SCENE_BLOCK_Y = (MAX_SCENE_Y / MIN_SCENE_BLOCK_SIZE);


#define MAX_PATH_NODE (MAX_SCENE_BLOCK_X * MAX_SCENE_BLOCK_Y)
#define MAX_PINBLOCK_DATA                (((1+(MAX_SCENE_X/MIN_SCENE_BLOCK_SIZE))*(1+MAX_SCENE_Y/MIN_SCENE_BLOCK_SIZE)) / 8 )
#define ABS(a,b)                         (((unsigned int) (a) > (unsigned int)(b)) ? ((a) - (b)) : ((b) - (a)))
#define MAX_POSITION_NUMBER_IN_PATH      1000
#define MIN_GRID_SIZE                    0.5
#define DEFAULT_GRID_SIZE                1.0f
#define BLOCK_FILE_VERSION               1

typedef struct tagUnitPosition
{
	float m_uiX;                  
	float m_uiY; 
}TUnitPosition;

typedef struct tagUnitPath
{
	unsigned char m_iNumber;               	/*   位置数量 */
	TUnitPosition m_astPosition[MAX_POSITION_NUMBER_IN_PATH];
}TUnitPath;

enum PathError
{
	PathError_None = 0,

	PathError_NoFile = -1,
	PathError_SceneName = -2,
	PathError_Invalid_GridSize = -3,
	PathError_Invalid_Column_Row_Count = -4,
	PathError_Invalid_Format = -5,
	PathError_Invalid_File_Length = -6 ,
	PathError_Invalid_Version = - 7
};

struct MAPINFO 
{
	//fourcc
	int m_fourcc;

	//版本号
	int m_Version;
	//场景id
	int m_iSceneID;

	//格子大小
	float m_iGridSize;

	// 场景的尺寸
	short m_iWidthPixels;
	short m_iHeightPixels;

	void Reset()
	{
		m_iWidthPixels = m_iHeightPixels = 0;
		m_iGridSize = 1 ;
	}
};

// A* 算法参数
struct tagSceneBlock;

typedef struct tagAStarNode
{
	unsigned short iValueG;
	unsigned short iValueF;   // FGH值

	bool bClosed;  // 是否在封闭链表
	bool bOpened;  // 是否在开放链表

	tagSceneBlock *pstCenterNode; // 中心节点

	void reset()
	{
		bClosed=false;
		bOpened=false;
		iValueG=0;
		iValueF=0;
		pstCenterNode=NULL;
	}

}TAStarNode;

// 地图块
typedef struct tagSceneBlock
{
	int   iWalkable;      // 是否可行走
	int   iSafeZone;      // 是否是安全区，默认不是安全区
	unsigned short iBlockIndex;      // 块层级, 第0级即最小可行走单位, 第1级包含4个0级块, 第2级包含4个1级块, 依次类推
	unsigned short iFirstBlock;      // 起始的0级块序号

	TAStarNode stAStar;   
}TSceneBlock;



// 路径点最小堆，优化A*算法的开放列表


class CPathMinHeap
{
public:
	CPathMinHeap();

	void Initialize();

	// 弹出路径最小的点
	TSceneBlock *PopHeap();

	// 压入一个路径点
	bool PushHeap(TSceneBlock *pstSceneBlock);

	// 判断是否缓冲区满
	bool IsHeapFull();

private:
	bool InsertHeap(TSceneBlock *pstSceneBlock, int iPosition);

private:
	int m_iOpenNodes;
//	int m_iCloseNodes;
	TSceneBlock *m_astOpenNode[MAX_PATH_NODE];
//	tagSceneBlock *m_astCloseNode[MAX_PATH_NODE];
};

class CScenePathManager
{
public:
	~CScenePathManager();
	//初始化路径图
	int Initalize(const char* blockFileName);

	int GetSceneSize(int& iWidthPixels, int& iHeightPixels);

	int GetSceneBlocks(int& iWidthBlocks, int& iHeightBlocks);

	int GetSceneID();

	// 安全区初始化
	int InitSafeZone( char * pSceneSafeZone, int iSceneID ) ;

public:
	bool CheckPoint(float x,float y);
	bool CheckPath(float x1,float y1,float x2,float y2);
	bool FindPath(float x1,float y1,float x2,float y2,TUnitPath &rstPath);
	//在地图中寻找与目标点在直线距离上最近的点;,z
	void FindCross( float x1, float y1, float x2, float y2, float* px, float* py ){}
	//设置阻挡片;
	void EnableGroupFunc(const wchar_t* name, unsigned int enable){}

public:

	// 是否是安全区坐标
	bool IsSafeZone(const TUnitPosition &rstPosition);

	// 验证路径点可达
	bool CanWalk(const TUnitPosition &rstPosition, char * pPinDynBlockData = NULL);

	// 验证两点之间是否可达 直线
	// bIgnoreEnd: 是否忽略终点坐标。因为单位在移动过程中， 可能“擦”在阻挡上， 所以技能阻挡判断时忽略之。
	bool CanWalk(const TUnitPosition &rstStartPosition, const TUnitPosition &rstEndPosition, bool bIgnoreEnd = false, bool bFromRobot = false, char * pPinDynBlockData = NULL);

	// 路径验证
	bool CanWalk(const TUnitPosition &rstStartPosition, const TUnitPath &rstPath, char * pPinDynBlockData = NULL, int * piValidPos = NULL);

	// A* 算法寻路
	bool FindPathSlow(const TUnitPosition &rstStartPosition, 
		const TUnitPosition &rstEndPosition, 
		TUnitPath &rstPath);

	void OptimizePath(const TUnitPosition &rstStartPosition, TUnitPath &rstPath);

	// 判断是不是阻挡点
	bool IsDynBlockMask(const TUnitPosition& rstPosition, char * pData); 

	// 判断是不是阻挡块
	bool IsDynBlockMask(int iBlockX, int iBlockY, char * pData); 

private:
	void ResetAstar();
	//加载阻挡
	int LoadBlock();
	// 块融合 
	void MergeSceneBlock();

	// 直线方程逼近验证 
	bool CanWalkSlow(int iStartBlockX, int iStartBlockY, int iEndBlockX, int iEndBlockY, bool bIgnoreEnd = false, char * pPinDynBlockData = NULL);

	// A* 计算节点权值
	int AStarCountNode(int iX, int iY, int iEndX, int iEndY, TSceneBlock *pstCenterBlock);

	// A* 路径优化
	void OptimizeAStarPath(const TUnitPosition &rstStartPosition, TUnitPath &rstPath);

	inline int Position2Block(float x){return x * m_blockCountPerUnit;}

public:
	MAPINFO m_mapinfo;
	int m_iSceneID;

	// 场景的尺寸
	int m_iWidthPixels;
	int m_iHeightPixels;

	int m_iWidthBlocks;
	int m_iHeightBlocks;

	// 最大路径融合层数
	int m_iMaxBlockIndex;   

	// 路径块
	TSceneBlock* m_astSceneBlock;

	// A* 最小堆
	static CPathMinHeap m_stPathMinHeap;

	float m_blockCountPerUnit;

	char *m_blockData;
};

#define BLOCK_X(pBlock) ( ((pBlock) - &m_astSceneBlock[0]) % (m_iWidthBlocks) )
#define BLOCK_Y(pBlock) ( ((pBlock) - &m_astSceneBlock[0]) / (m_iWidthBlocks) )

#endif

