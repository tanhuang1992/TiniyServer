#include "stdafx.h"
#include "ScenePathManager.h"
#include <string.h>
#include <stdlib.h>

// 寻路临时路径点数组
static int g_iTempPathNumber;
static const int MAX_TEMP_PATH_NUMBER = 1024;
static TUnitPosition g_astTempPosition[MAX_TEMP_PATH_NUMBER];
CPathMinHeap CScenePathManager::m_stPathMinHeap;


int CScenePathManager::GetSceneID()
{
    return m_iSceneID;
}


int CScenePathManager::GetSceneSize(int& iWidthPixels, int& iHeightPixels)
{
    iWidthPixels = m_iWidthPixels;
    iHeightPixels = m_iHeightPixels;

    return 0;
}

int CScenePathManager::GetSceneBlocks(int& iWidthBlocks, int& iHeightBlocks)
{
    iWidthBlocks = m_iWidthBlocks;
    iHeightBlocks = m_iHeightBlocks;

    return 0;
}

CScenePathManager::~CScenePathManager()
{
	free(m_blockData);
}

int CScenePathManager::Initalize(const char* blockFileName)
{
	int iSize;
	// 打开文件
	FILE *pSceneFile = ::fopen(blockFileName, "rb");
// 	FILE *pSceneFile; 
// 	fopen_s(&pSceneFile,blockFileName, "rb");
	if(!pSceneFile)
	{
		return PathError_NoFile;
	}

	//fourcc，ID，版本
	fread(&m_mapinfo.m_fourcc,sizeof(int),1,pSceneFile);

	//todo ,check block file version
	iSize=fread(&m_mapinfo.m_Version, sizeof(int), 1, pSceneFile);
	iSize=fread(&m_mapinfo.m_iSceneID, sizeof(int), 1, pSceneFile);

	// 地图尺寸
	iSize = fread(&m_mapinfo.m_iWidthPixels, sizeof(unsigned short), 1, pSceneFile);
	iSize = fread(&m_mapinfo.m_iHeightPixels, sizeof(unsigned short), 1, pSceneFile);

	//每个格子多少像素点
	iSize=fread((float*)&m_mapinfo.m_iGridSize,sizeof(float),1,pSceneFile);	
	if (m_mapinfo.m_iGridSize < MIN_GRID_SIZE)
	{
		m_mapinfo.Reset();
		::fclose(pSceneFile);
		return PathError_Invalid_GridSize;
	}

	m_iSceneID = m_mapinfo.m_iSceneID;

	m_iWidthPixels = m_mapinfo.m_iWidthPixels;
	m_iHeightPixels = m_mapinfo.m_iHeightPixels;

	m_blockCountPerUnit=1.0/m_mapinfo.m_iGridSize;
	m_iWidthBlocks = Position2Block(m_iWidthPixels); 
	m_iHeightBlocks = Position2Block(m_iHeightPixels); 

	// 阻挡数据
	int iPathBlockLength=m_iWidthBlocks*m_iHeightBlocks;
	m_blockData = (char *)::malloc(iPathBlockLength);
	iSize = ::fread(m_blockData, iPathBlockLength, 1, pSceneFile);
	::fclose(pSceneFile);

	LoadBlock();
	return PathError_None;
}

int CScenePathManager::LoadBlock()
{
	m_astSceneBlock = new TSceneBlock[m_iWidthBlocks * m_iHeightBlocks];

    int iX, iY;
    for (iY = 0; iY < m_iHeightBlocks; iY++)
    {
        for (iX = 0; iX < m_iWidthBlocks; iX++)
        {
			int iBlock = iY * m_iWidthBlocks + iX;
            m_astSceneBlock[iBlock].iFirstBlock = iBlock;

            m_astSceneBlock[iBlock].iSafeZone = 0;  // 默认都不是安全区
            if (m_blockData[iBlock] == 0)
            {
                m_astSceneBlock[iBlock].iWalkable = 1 ;
            }
            else
            {
                m_astSceneBlock[iBlock].iWalkable = 0 ;
            }
            m_astSceneBlock[iBlock].iBlockIndex = 0; 
			m_astSceneBlock[iBlock].stAStar.reset();
        }
    }
    
    m_iMaxBlockIndex = 0;
    MergeSceneBlock();

    return 0;
}

/***********************************************************
  初始化安全区
 **********************************************************/
int CScenePathManager::InitSafeZone( char * pSceneSafeZone, int iSceneID ) 
{
    int iX, iY;
    for (iY = 0; iY < m_iHeightBlocks; iY++)
    {
        for (iX = 0; iX < m_iWidthBlocks; iX++)
        {
			int iBlock = iY * m_iWidthBlocks + iX;
            if (pSceneSafeZone[iBlock] == 0)
            {
                m_astSceneBlock[iBlock].iSafeZone = 0 ;
            }
            else
            {
                m_astSceneBlock[iBlock].iSafeZone = 1 ;
            }
        }
    }

    return 0 ;
}

//融合四个相邻的块，形成一个大块, 并不断递归下去，直到不能融合
void CScenePathManager::MergeSceneBlock()
{
    // 使用静态变量防止递归堆栈溢出 
    static int iMerged;
    static int iBlockSize;
    static int iWidthBlocks;
    static int iHeightBlocks;
    static int iX, iY;
    
    // 融合的块数，如果结果是0，表示融合过程结束 
    iMerged = 0;

    // 当前层级下块的大小 
    m_iMaxBlockIndex++;
    iBlockSize = 1 << m_iMaxBlockIndex;

    // 当前层级下的块数目 
    iWidthBlocks = m_iWidthBlocks / iBlockSize;
    iHeightBlocks = m_iHeightBlocks / iBlockSize;

    for (iY = 0; iY < iHeightBlocks; iY++)
    {
        for (iX = 0; iX < iWidthBlocks; iX++)
        {
            // 尝试融合一个大块 
            int iFirstBlockX = iX * iBlockSize;
            int iFirstBlockY = iY * iBlockSize;
            int iFirstBlock = iFirstBlockY * m_iWidthBlocks + iFirstBlockX;

            int iBlockWalkable;
            int iBlockX, iBlockY;
            for (iBlockY = 0; iBlockY < iBlockSize; iBlockY++)
            {
                for (iBlockX = 0; iBlockX < iBlockSize; iBlockX++)
                {
					int iBlockIndex = (iFirstBlockY + iBlockY) * m_iWidthBlocks + (iFirstBlockX + iBlockX);
                    iBlockWalkable = m_astSceneBlock[iBlockIndex].iWalkable;
                    if (iBlockWalkable == 0)
                    {
                        break;
                    }
                }

                if (iBlockWalkable == 0)
                {
                    break;
                }
            }

            if (iBlockWalkable == 0)
            {
                continue;
            }

            // 融合所有小块到大块 
            for (iBlockY = 0; iBlockY < iBlockSize; iBlockY++)
            {
                for (iBlockX = 0; iBlockX < iBlockSize; iBlockX++)
                {
					int iBlockIndex = (iFirstBlockY + iBlockY) * m_iWidthBlocks + (iFirstBlockX + iBlockX);
                    TSceneBlock &block = m_astSceneBlock[iBlockIndex];
                    block.iBlockIndex = m_iMaxBlockIndex;
                    block.iFirstBlock = iFirstBlock;
                }
            }

            iMerged++;
        }
    }

    if (iMerged >= 4)
    {
        return MergeSceneBlock();
    }

 /*   LOGDEBUG("Map blocks merged %d times\n", m_iMaxBlockIndex - 1);*/

    return;
}


bool CScenePathManager::CanWalk(const TUnitPosition &rstStartPosition, const TUnitPath &rstPath, char * pPinDynBlockData, int * piValidPos)
{
    if (rstPath.m_iNumber <= 0 || rstPath.m_iNumber >= MAX_POSITION_NUMBER_IN_PATH)
    {
        return false;
    }

    if (!CanWalk(rstStartPosition, rstPath.m_astPosition[0], false, false, pPinDynBlockData))
    {
        return false;
    }

    for (int i = 0; i < rstPath.m_iNumber - 1; i++)
    {
        if (!CanWalk(rstPath.m_astPosition[i], rstPath.m_astPosition[i+1], false, false, pPinDynBlockData))
        {
            if (piValidPos)
            {
                *piValidPos = i ;
            }

            return false;
        }
    }

    return true;
}

// 是否在安全区
bool CScenePathManager::IsSafeZone(const TUnitPosition &rstPosition)
{
    int iBlockX = Position2Block(rstPosition.m_uiX);
    int iBlockY = Position2Block(rstPosition.m_uiY);

    if (iBlockX < 0 || iBlockX >= m_iWidthBlocks || 
        iBlockY < 0 || iBlockY >= m_iHeightBlocks)
    {
        return false;
    }

    return m_astSceneBlock[iBlockY * m_iWidthBlocks + iBlockX].iSafeZone != 0;
}

bool CScenePathManager::IsDynBlockMask(const TUnitPosition& rstPosition, char * pData)
{
    if (!pData)
    {
        return false; 
    }

    int iBlockX = Position2Block(rstPosition.m_uiX);
    int iBlockY = Position2Block(rstPosition.m_uiY);

    return IsDynBlockMask(iBlockX, iBlockY, pData) ;
}

bool CScenePathManager::IsDynBlockMask(int iBlockX, int iBlockY, char * pData)
{
    if (!pData)
    {
        return false; 
    }

    int iBlock  = iBlockY * m_iWidthBlocks + iBlockX;

    int iPos = iBlock / 8;
    int iBit = iBlock % 8;
    unsigned char ucFlag = (1 << (iBit)) ;

    if (iPos > MAX_PINBLOCK_DATA)
    {
        return false ;  
    }

    bool bRet = (pData[iPos] & ucFlag) ; 
    return bRet;
}



bool CScenePathManager::CanWalk(const TUnitPosition &rstPosition, char * pPinDynBlockData)
{
    int iBlockX = Position2Block(rstPosition.m_uiX);
    int iBlockY = Position2Block(rstPosition.m_uiY);

    if (iBlockX < 0 || iBlockX >= m_iWidthBlocks || 
        iBlockY < 0 || iBlockY >= m_iHeightBlocks)
    {
        return false;
    }

    // 加判下动态阻挡。
    if (pPinDynBlockData && IsDynBlockMask(rstPosition, pPinDynBlockData))
    {
        return false ;   // 不能走。
    }

    return m_astSceneBlock[iBlockY * m_iWidthBlocks + iBlockX].iWalkable != 0;
}
   
bool CScenePathManager::CanWalk(const TUnitPosition &rstStartPosition,
								const TUnitPosition &rstEndPosition, bool bIgnoreEnd, bool bFromRobot, char * pPinDynBlockData)
{
    int iStartBlockX = Position2Block(rstStartPosition.m_uiX);
    int iStartBlockY = Position2Block(rstStartPosition.m_uiY);

    int iEndBlockX = Position2Block(rstEndPosition.m_uiX);
    int iEndBlockY = Position2Block(rstEndPosition.m_uiY);

    if (iStartBlockX < 0 || iStartBlockX >= m_iWidthBlocks 
        || iStartBlockY < 0 || iStartBlockY >= m_iHeightBlocks
        || iEndBlockX < 0 || iEndBlockX >= m_iWidthBlocks 
        || iEndBlockY < 0 || iEndBlockY >= m_iHeightBlocks)
    {
        return false;
    }

    TSceneBlock &stStartBlock = m_astSceneBlock[iStartBlockY * m_iWidthBlocks + iStartBlockX];
    TSceneBlock &stEndBlock = m_astSceneBlock[iEndBlockY * m_iWidthBlocks + iEndBlockX];


    // 在优化后的行进路径上会”蹭”到不可行走的点，所以对起点不做判断, 只判断终点
    if (!bIgnoreEnd && stEndBlock.iWalkable == 0)
    {
        return false;
    }

    // 起点和终点在一个大块中， 有动态阻挡的话，这个对场景的优化算法无效。
    if (stStartBlock.iFirstBlock == stEndBlock.iFirstBlock && !pPinDynBlockData)
    {
        return true;
    }

    // 按照直线方程进行检测 
    return CanWalkSlow(iStartBlockX, iStartBlockY, iEndBlockX, iEndBlockY, bIgnoreEnd, pPinDynBlockData);
}


bool CScenePathManager::CanWalkSlow(int iStartBlockX, int iStartBlockY, 
									int iEndBlockX, int iEndBlockY,
									bool bIgnoreEnd, char * pPinDynBlockData) 
{
	if ((iStartBlockX == iEndBlockX)&&(iStartBlockY == iEndBlockY))
	{
		return true;
	}
    // 沿着X轴步进还是Y轴步进
    int iWidthBlocks = ABS(iEndBlockX, iStartBlockX);
    int iHeightBlocks = ABS(iEndBlockY, iStartBlockY);

	if (bIgnoreEnd)
	{
		iWidthBlocks--;
		iHeightBlocks--;
	}

    bool bStepX = iWidthBlocks >= iHeightBlocks;

    // X，Y坐标增加还是减少
    bool bGrowX = iEndBlockX > iStartBlockX;
    bool bGrowY = iEndBlockY > iStartBlockY;

    // 设置直线两端点为格子的中心坐标
    int iStartX = iStartBlockX * m_mapinfo.m_iGridSize + m_mapinfo.m_iGridSize/2;
    int iStartY = iStartBlockY * m_mapinfo.m_iGridSize + m_mapinfo.m_iGridSize/2;

    int iEndX = iEndBlockX * m_mapinfo.m_iGridSize + m_mapinfo.m_iGridSize/2;
    int iEndY = iEndBlockY * m_mapinfo.m_iGridSize + m_mapinfo.m_iGridSize/2;

    double K;        // 直线斜率
    double offset;   // 坐标偏移

    if (bStepX)
    {
        K = (double)(iEndY - iStartY) / (double)(iEndX - iStartX);
        offset = (double)(iStartY * iEndX - iEndY * iStartX) / (double)(iEndX - iStartX);

        for (int i = 0; i < iWidthBlocks; i++)
        {
            if (bGrowX)
            {
                iStartBlockX++;
            }
            else 
            {
                iStartBlockX--;
            }

            iStartX = iStartBlockX * m_mapinfo.m_iGridSize + m_mapinfo.m_iGridSize/2;
            iStartY = (int)(K * iStartX + offset + 0.5f);
            
            iStartBlockY = Position2Block(iStartY);
            

            // 加判下动态阻挡。
            if (pPinDynBlockData && IsDynBlockMask(iStartBlockX, iStartBlockY, pPinDynBlockData))
            {
//              IsDynBlockMask(iStartBlockX, iStartBlockY, pPinDynBlockData);
                return false ;   // 不能走。
            }

            if (m_astSceneBlock[iStartBlockY * m_iWidthBlocks + iStartBlockX].iWalkable == 0)
            {
                return false;
            }
        }
    }
    else
    {
        K = (double)(iEndX - iStartX) / (double)(iEndY - iStartY);
        offset = (double)(iEndY * iStartX - iStartY * iEndX) / (double)(iEndY - iStartY);
 
        for (int i = 0; i < iHeightBlocks; i++)
        {
            if (bGrowY)
            {
                iStartBlockY++;
            }
            else {
                iStartBlockY--;
            }
		
            iStartY = iStartBlockY * m_mapinfo.m_iGridSize + m_mapinfo.m_iGridSize/2;
            iStartX = (int)(K * iStartY + offset + 0.5f);

            iStartBlockX = Position2Block(iStartX);

            // 加判下动态阻挡。
            if (pPinDynBlockData && IsDynBlockMask(iStartBlockX, iStartBlockY, pPinDynBlockData))
            {
                return false ;   // 不能走。
            }

            if (m_astSceneBlock[iStartBlockY * m_iWidthBlocks + iStartBlockX].iWalkable == 0)
            {
                return false;
            }
        }
    }

    return true;
}

void CScenePathManager::ResetAstar()
{
	int iX, iY;
	for (iY = 0; iY < m_iHeightBlocks; iY++)
	{
		for (iX = 0; iX < m_iWidthBlocks; iX++)
		{
			int iBlock = iY * m_iWidthBlocks + iX;
			m_astSceneBlock[iBlock].stAStar.reset();
		}
	}
}


bool CScenePathManager::FindPathSlow(const TUnitPosition &rstStartPosition, const TUnitPosition &rstEndPosition, TUnitPath &rstPath)
{
	ResetAstar();
    int iStartBlockX = Position2Block(rstStartPosition.m_uiX);
    int iStartBlockY = Position2Block(rstStartPosition.m_uiY);

    int iEndBlockX = Position2Block(rstEndPosition.m_uiX);
    int iEndBlockY = Position2Block(rstEndPosition.m_uiY);

    if (iStartBlockX < 0 || iStartBlockX >= m_iWidthBlocks 
        || iStartBlockY < 0 || iStartBlockY >= m_iHeightBlocks
        || iEndBlockX < 0 || iEndBlockX >= m_iWidthBlocks 
        || iEndBlockY < 0 || iEndBlockY >= m_iHeightBlocks)
    {
        return false;
    }

    TSceneBlock *pstStartBlock = &m_astSceneBlock[iStartBlockY * m_iWidthBlocks + iStartBlockX];
    TSceneBlock *pstEndBlock = &m_astSceneBlock[iEndBlockY * m_iWidthBlocks + iEndBlockX];

    m_stPathMinHeap.Initialize();

    // 依次处理当前节点的周围4个节点, 直到到达最后节点
    TSceneBlock *pstCenterBlock = pstStartBlock;
    while (1)
    {
		if (m_stPathMinHeap.IsHeapFull())
		{
			break;
		}

        // 将当前节点加入封闭列表
        pstCenterBlock->stAStar.bClosed = true;

        int iCenterX = BLOCK_X(pstCenterBlock);
        int iCenterY = BLOCK_Y(pstCenterBlock);

        // 到达终点所属的块
        if (pstCenterBlock->iFirstBlock == pstEndBlock->iFirstBlock)
        {
            break;
        }

        if (iCenterX - 1 >= 0)
        {
            int iRet = AStarCountNode(iCenterX - 1, iCenterY, iEndBlockX, iEndBlockY, pstCenterBlock);
			if (iRet < 0)
			{
				break;
			}
        }

        if (iCenterX + 1 < m_iWidthBlocks)
        {
            int iRet = AStarCountNode(iCenterX + 1, iCenterY, iEndBlockX, iEndBlockY, pstCenterBlock);
			if (iRet < 0)
			{
				break;
			}
        }

        if (iCenterY - 1 >= 0)
        {
            int iRet = AStarCountNode(iCenterX, iCenterY - 1, iEndBlockX, iEndBlockY, pstCenterBlock);
			if (iRet < 0)
			{
				break;
			}
        }

        if (iCenterY + 1 < m_iHeightBlocks)
        {
            int iRet = AStarCountNode(iCenterX, iCenterY + 1, iEndBlockX, iEndBlockY, pstCenterBlock);
			if (iRet < 0)
			{
				break;
			}
        }

        // 取开放列表中路径最小的作为当前节点
        pstCenterBlock = m_stPathMinHeap.PopHeap();
        if (pstCenterBlock == NULL)
        {
            pstStartBlock->stAStar.bClosed = false;
            break;
        }    
    }

    pstStartBlock->stAStar.bClosed = false;

    // A* 算法结束, 进行路径优化

    // 去掉终点块
    if (pstCenterBlock == pstEndBlock)
    {
        pstCenterBlock = pstCenterBlock->stAStar.pstCenterNode;
    }

    // 统计路径点个数
    g_iTempPathNumber = 0;
    TSceneBlock *pstPathBlock = pstCenterBlock;
    while (pstPathBlock && pstPathBlock != pstStartBlock)
    {
        g_iTempPathNumber++;
        pstPathBlock = pstPathBlock->stAStar.pstCenterNode;
    }

    if (g_iTempPathNumber >= MAX_TEMP_PATH_NUMBER - 1)
    {
        return false;
    } 

    // 将路径点反向连起来
    pstPathBlock = pstCenterBlock;
    for (int i = g_iTempPathNumber - 1; i >= 0; i--)
    {
        int iCenterX = BLOCK_X(pstPathBlock) * m_mapinfo.m_iGridSize + m_mapinfo.m_iGridSize / 2;
        int iCenterY = BLOCK_Y(pstPathBlock) * m_mapinfo.m_iGridSize + m_mapinfo.m_iGridSize / 2;

        g_astTempPosition[i].m_uiX = iCenterX;
        g_astTempPosition[i].m_uiY = iCenterY;

        pstPathBlock = pstPathBlock->stAStar.pstCenterNode;
    }

    // 添加终点
    g_iTempPathNumber++;
    g_astTempPosition[g_iTempPathNumber - 1] = rstEndPosition;

    OptimizeAStarPath(rstStartPosition, rstPath);

    return true;
}


int CScenePathManager::AStarCountNode(int iX, int iY, int iEndX, int iEndY, TSceneBlock *pstCenterBlock)
{
    TSceneBlock *pstNeighborBlock = &m_astSceneBlock[iY * m_iWidthBlocks + iX];

    // 此块不可行走，忽略
    if (pstNeighborBlock->iWalkable == 0)
    {
        return 0;
    }

    TAStarNode &stAStar = pstNeighborBlock->stAStar;

    // 已经加入封闭列表，忽略
    if (stAStar.bClosed)
    {
        return 0;
    }

    //省略G参数可以提高速度, 但得不到最优路径, 服务器做怪物短路径寻路不碍事
    int iValueG = pstCenterBlock->stAStar.iValueG + 1;

    // 在开放列表中，但路径比现在优先，忽略
    if (stAStar.bOpened && stAStar.iValueG < iValueG)
    {
        return 0;
    }

    // 更新当前节点GHF值，并指向新的中心节点
    stAStar.iValueG = iValueG;
    // stAStar.iValueH = ABS(iX, iEndX) + ABS(iY, iEndY);
    stAStar.iValueF = stAStar.iValueG + ABS(iX, iEndX) + ABS(iY, iEndY);

    stAStar.pstCenterNode = pstCenterBlock;

    // 加入开放列表
    if (!stAStar.bOpened)
    {
        bool bPushed = m_stPathMinHeap.PushHeap(pstNeighborBlock);
		if (!bPushed)
		{
			return -1;	
		}

		stAStar.bOpened = true;
    }

    return 0;
}

void CScenePathManager::OptimizeAStarPath(const TUnitPosition &rstStartPosition, TUnitPath &rstPath)
{
    int iSavedNumber = g_iTempPathNumber;

    // 优化起始点
    int iFirstPoint = 0;
    int iSecondPoint = iFirstPoint + 1;
    while (g_iTempPathNumber > 1 && CanWalk(rstStartPosition, g_astTempPosition[iSecondPoint]))
    {       
        iFirstPoint = iSecondPoint;
        iSecondPoint = iFirstPoint + 1;
        g_iTempPathNumber--;
    }

    int iSavedFirstPoint = iFirstPoint;

    // 优化中间路径点
    int iThirdPoint;

    while (g_iTempPathNumber > 2)
    {
        iThirdPoint = iSecondPoint + 1;

        if (iThirdPoint >= iSavedNumber)
        {
            break;
        }

        if (CanWalk(g_astTempPosition[iFirstPoint], g_astTempPosition[iThirdPoint]))
        {
            g_astTempPosition[iSecondPoint].m_uiX = (unsigned int)-1;

            iSecondPoint = iThirdPoint;

            g_iTempPathNumber--;
            continue;
        }

        iFirstPoint = iSecondPoint;
        iSecondPoint = iFirstPoint + 1;
    }

    // 收集所有有效路径点
    int i = iSavedFirstPoint; 
    rstPath.m_iNumber = 0;
    while (rstPath.m_iNumber < g_iTempPathNumber)
    {
        if (g_astTempPosition[i].m_uiX != (unsigned int)-1)
        {
            rstPath.m_astPosition[rstPath.m_iNumber++] = g_astTempPosition[i];
            if (rstPath.m_iNumber >= MAX_POSITION_NUMBER_IN_PATH - 1)
            {
                break;
            }
        }     

        i++;
    }
}

bool CScenePathManager::CheckPoint(float x,float y)
{
	TUnitPosition rstpos;
	rstpos.m_uiX=x;
	rstpos.m_uiY=y;

	return CanWalk(rstpos);
}

bool CScenePathManager::CheckPath(float x1,float y1,float x2,float y2)
{
	TUnitPosition rstpos;
	rstpos.m_uiX=x1;
	rstpos.m_uiY=y1;
	TUnitPosition endpos;
	endpos.m_uiX=x2;
	endpos.m_uiY=y2;

	return CanWalk(rstpos,endpos);
}

bool CScenePathManager::FindPath(float x1,float y1,float x2,float y2,TUnitPath &rstPath)
{
	TUnitPosition rstpos;
	rstpos.m_uiX=x1;
	rstpos.m_uiY=y1;
	TUnitPosition endpos;
	endpos.m_uiX=x2;
	endpos.m_uiY=y2;

	return FindPathSlow(rstpos,endpos,rstPath);
}

void CScenePathManager::OptimizePath(const TUnitPosition &rstStartPosition, TUnitPath &rstPath)
{
    int iSavedNumber = rstPath.m_iNumber;

    // 优化路径的起始点
    // 如果当前位置点能直接到达第二个路径点, 则将第二个路径点设置为第一个路径点
    int iFirstPoint = 0;
    int iSecondPoint = iFirstPoint + 1;
    while (rstPath.m_iNumber > 1 && CanWalk(rstStartPosition, rstPath.m_astPosition[iSecondPoint]))
    {       
        iFirstPoint = iSecondPoint;
        iSecondPoint = iFirstPoint + 1;
        rstPath.m_iNumber--;
    }

    int iSavedFirstPoint = iFirstPoint;

    // 优化中间路径点
    // 如果第一个点能直接到达第三个点,  则去除第二个点
    int iThirdPoint;
    int iFourthPoint;

    while (rstPath.m_iNumber > 2)
    {
        iThirdPoint = iSecondPoint + 1;
        iFourthPoint = iThirdPoint + 1;

        if (iThirdPoint >= iSavedNumber)
        {
            break;
        }

        if (iFourthPoint < iSavedNumber && CanWalk(
            rstPath.m_astPosition[iFirstPoint], 
            rstPath.m_astPosition[iFourthPoint]))
        {
            rstPath.m_astPosition[iSecondPoint].m_uiX = (unsigned int)-1;
            rstPath.m_astPosition[iThirdPoint].m_uiX = (unsigned int)-1;

            iSecondPoint = iFourthPoint;

            rstPath.m_iNumber--;
            rstPath.m_iNumber--;
            continue;
        }

        if (CanWalk(rstPath.m_astPosition[iFirstPoint], rstPath.m_astPosition[iThirdPoint]))
        {
            rstPath.m_astPosition[iSecondPoint].m_uiX = (unsigned int)-1;

            iSecondPoint = iThirdPoint;

            rstPath.m_iNumber--;
            continue;
        }

        iFirstPoint = iSecondPoint;
        iSecondPoint = iFirstPoint + 1;
    }

    // 收集所有有效路径点, 无效的路径点X左边已经被置为-1
    int i = iSavedFirstPoint; 
    int j = 0;
    while (j < rstPath.m_iNumber)
    {
        if (rstPath.m_astPosition[i].m_uiX != (unsigned int)-1)
        {
            rstPath.m_astPosition[j++] = rstPath.m_astPosition[i];
        }     

        i++;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////
CPathMinHeap::CPathMinHeap()
{
    m_iOpenNodes = 0;
    memset(m_astOpenNode, 0, sizeof(m_astOpenNode));
}

// 判断是否缓冲区满
bool CPathMinHeap::IsHeapFull()
{
	return (m_iOpenNodes >= MAX_PATH_NODE);
}

void CPathMinHeap::Initialize()
{
    for (int i = 0; i < m_iOpenNodes; i++)
    {
		m_astOpenNode[i]->stAStar.reset();
    }

    m_iOpenNodes = 0;
}

TSceneBlock *CPathMinHeap::PopHeap()
{
    if (m_iOpenNodes <= 0)
    {
        return NULL;
    }

    // 保存最小值
    TSceneBlock *pstMinBlock = m_astOpenNode[0];

    // 比较两个子节点，将小的提升为父节点
    int iParent = 0;
    int iLeftChild, iRightChild;
    for (iLeftChild = 2 * iParent + 1, iRightChild = iLeftChild + 1;
        iRightChild < m_iOpenNodes;
        iLeftChild = 2 * iParent + 1, iRightChild = iLeftChild + 1)
    {
        if (m_astOpenNode[iLeftChild]->stAStar.iValueF < m_astOpenNode[iRightChild]->stAStar.iValueF)
        {
            m_astOpenNode[iParent] = m_astOpenNode[iLeftChild];
            iParent = iLeftChild;
        }
        else
        {
            m_astOpenNode[iParent] = m_astOpenNode[iRightChild];
            iParent = iRightChild;
        }
    }

    // 将最后一个节点填在空出来的节点上, 防止数组空洞
    if (iParent != m_iOpenNodes - 1)
    {
        bool bPushed = InsertHeap(m_astOpenNode[--m_iOpenNodes], iParent);
		if (!bPushed)
		{
			return NULL;
		}
    }
    m_iOpenNodes--;

    return pstMinBlock;
}

bool CPathMinHeap::PushHeap(TSceneBlock *pstSceneBlock)
{
    if (m_iOpenNodes >= MAX_PATH_NODE)
    {
        return false;
    }

    return InsertHeap(pstSceneBlock, m_iOpenNodes);
}

bool CPathMinHeap::InsertHeap(TSceneBlock *pstSceneBlock, int iPosition)
{
	if (iPosition >= MAX_PATH_NODE)
	{
		return false;
	}

    m_astOpenNode[iPosition] = pstSceneBlock;

    // 依次和父节点比较，如果比父节点小，则上移
    int iChild, iParent;
    for (iChild = iPosition, iParent = (iChild - 1) / 2;
        iChild > 0;
        iChild = iParent, iParent = (iChild - 1) / 2)
    {
        if (m_astOpenNode[iChild]->stAStar.iValueF < m_astOpenNode[iParent]->stAStar.iValueF)
        {
            TSceneBlock *tmp = m_astOpenNode[iParent];
            m_astOpenNode[iParent] = m_astOpenNode[iChild];
            m_astOpenNode[iChild] = tmp;
        }
        else
        {
            break;
        }
    }

    m_iOpenNodes++;

    return true;
}
