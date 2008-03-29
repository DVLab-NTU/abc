/**CFile****************************************************************

  FileName    [mfsCore.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [The good old minimization with complete don't-cares.]

  Synopsis    [Core procedures of this package.]

  Author      [Alan Mishchenko]
  
  Affiliation [UC Berkeley]

  Date        [Ver. 1.0. Started - June 20, 2005.]

  Revision    [$Id: mfsCore.c,v 1.00 2005/06/20 00:00:00 alanmi Exp $]

***********************************************************************/

#include "mfsInt.h"

////////////////////////////////////////////////////////////////////////
///                        DECLARATIONS                              ///
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////

/**Function*************************************************************

  Synopsis    []

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
int Abc_NtkMfsResub( Mfs_Man_t * p, Abc_Obj_t * pNode )
{
    int clk;
    p->nNodesTried++;
    // prepare data structure for this node
    Mfs_ManClean( p ); 
    // compute window roots, window support, and window nodes
clk = clock();
    p->vRoots = Abc_MfsComputeRoots( pNode, p->pPars->nWinTfoLevs, p->pPars->nFanoutsMax );
    p->vSupp  = Abc_NtkNodeSupport( p->pNtk, (Abc_Obj_t **)Vec_PtrArray(p->vRoots), Vec_PtrSize(p->vRoots) );
    p->vNodes = Abc_NtkDfsNodes( p->pNtk, (Abc_Obj_t **)Vec_PtrArray(p->vRoots), Vec_PtrSize(p->vRoots) );
p->timeWin += clock() - clk;
    if ( p->pPars->nWinSizeMax && Vec_PtrSize(p->vNodes) > p->pPars->nWinSizeMax )
        return 1;
    // compute the divisors of the window
clk = clock();
    p->vDivs  = Abc_MfsComputeDivisors( p, pNode, Abc_ObjRequiredLevel(pNode) - 1 );
    p->nTotalDivs += Vec_PtrSize(p->vDivs);
p->timeDiv += clock() - clk;
    // construct AIG for the window
clk = clock();
    p->pAigWin = Abc_NtkConstructAig( p, pNode );
p->timeAig += clock() - clk;
    // translate it into CNF
clk = clock();
    p->pCnf = Cnf_DeriveSimple( p->pAigWin, 1 + Vec_PtrSize(p->vDivs) );
p->timeCnf += clock() - clk;
    // create the SAT problem
clk = clock();
    p->pSat = Abc_MfsCreateSolverResub( p, NULL, 0, 0 );
    if ( p->pSat == NULL )
    {
        p->nNodesBad++;
        return 1;
    }
    // solve the SAT problem
    if ( p->pPars->fSwapEdge )
        Abc_NtkMfsEdgeSwapEval( p, pNode );
    else
    {
        Abc_NtkMfsResubNode( p, pNode );
        if ( p->pPars->fMoreEffort )
            Abc_NtkMfsResubNode2( p, pNode );
    }
p->timeSat += clock() - clk;
    return 1;
}

/**Function*************************************************************

  Synopsis    []

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
int Abc_NtkMfsNode( Mfs_Man_t * p, Abc_Obj_t * pNode )
{
    Hop_Obj_t * pObj;
    int RetValue;
    extern Hop_Obj_t * Abc_NodeIfNodeResyn( Bdc_Man_t * p, Hop_Man_t * pHop, Hop_Obj_t * pRoot, int nVars, Vec_Int_t * vTruth, unsigned * puCare );

    int nGain, clk;
    p->nNodesTried++;
    // prepare data structure for this node
    Mfs_ManClean( p );
    // compute window roots, window support, and window nodes
clk = clock();
    p->vRoots = Abc_MfsComputeRoots( pNode, p->pPars->nWinTfoLevs, p->pPars->nFanoutsMax );
    p->vSupp  = Abc_NtkNodeSupport( p->pNtk, (Abc_Obj_t **)Vec_PtrArray(p->vRoots), Vec_PtrSize(p->vRoots) );
    p->vNodes = Abc_NtkDfsNodes( p->pNtk, (Abc_Obj_t **)Vec_PtrArray(p->vRoots), Vec_PtrSize(p->vRoots) );
p->timeWin += clock() - clk;
    // count the number of patterns
//    p->dTotalRatios += Abc_NtkConstraintRatio( p, pNode );
    // construct AIG for the window
clk = clock();
    p->pAigWin = Abc_NtkConstructAig( p, pNode );
p->timeAig += clock() - clk;
    // translate it into CNF
clk = clock();
    p->pCnf = Cnf_DeriveSimple( p->pAigWin, Abc_ObjFaninNum(pNode) );
p->timeCnf += clock() - clk;
    // create the SAT problem
clk = clock();
    p->pSat = Cnf_DataWriteIntoSolver( p->pCnf, 1, 0 );
    if ( p->pSat == NULL )
        return 0;
    // solve the SAT problem
    RetValue = Abc_NtkMfsSolveSat( p, pNode );
    p->nTotConfLevel += p->pSat->stats.conflicts;
p->timeSat += clock() - clk;
    if ( RetValue == 0 )
    {
        p->nTimeOutsLevel++;
        p->nTimeOuts++;
        return 0;
    }
    // minimize the local function of the node using bi-decomposition
    assert( p->nFanins == Abc_ObjFaninNum(pNode) );
    pObj = Abc_NodeIfNodeResyn( p->pManDec, pNode->pNtk->pManFunc, pNode->pData, p->nFanins, p->vTruth, p->uCare );
    nGain = Hop_DagSize(pNode->pData) - Hop_DagSize(pObj);
    if ( nGain >= 0 )
    {
        p->nNodesDec++;
        p->nNodesGained += nGain;
        p->nNodesGainedLevel += nGain;
        pNode->pData = pObj;    
    }
    return 1;
}

/**Function*************************************************************

  Synopsis    []

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
int Abc_NtkMfs( Abc_Ntk_t * pNtk, Mfs_Par_t * pPars )
{
    extern Aig_Man_t * Abc_NtkToDar( Abc_Ntk_t * pNtk, int fRegisters );

    Bdc_Par_t Pars = {0}, * pDecPars = &Pars;
    ProgressBar * pProgress;
    Mfs_Man_t * p;
    Abc_Obj_t * pObj;
    Vec_Vec_t * vLevels;
    Vec_Ptr_t * vNodes;
    int i, k, nNodes, nFaninMax, clk = clock(), clk2;
    int nTotalNodesBeg = Abc_NtkNodeNum(pNtk);
    int nTotalEdgesBeg = Abc_NtkGetTotalFanins(pNtk);

    assert( Abc_NtkIsLogic(pNtk) );
    nFaninMax = Abc_NtkGetFaninMax(pNtk);
    if ( pPars->fResub )
    {
        if ( nFaninMax > 8 )
        {
            printf( "Nodes with more than %d fanins will node be processed.\n", 8 );
            nFaninMax = 8;
        }
    }
    else
    {
        if ( nFaninMax > MFS_FANIN_MAX )
        {
            printf( "Nodes with more than %d fanins will node be processed.\n", MFS_FANIN_MAX );
            nFaninMax = MFS_FANIN_MAX;
        }
    }
    // perform the network sweep
    Abc_NtkSweep( pNtk, 0 );
    // convert into the AIG
    if ( !Abc_NtkToAig(pNtk) )
    {
        fprintf( stdout, "Converting to BDD has failed.\n" );
        return 0;
    }
    assert( Abc_NtkHasAig(pNtk) );

    // start the manager
    p = Mfs_ManAlloc( pPars );
    p->pNtk = pNtk;
    p->nFaninMax = nFaninMax;
    if ( pNtk->pExcare )
    {
        Abc_Ntk_t * pTemp;
        pTemp = Abc_NtkStrash( pNtk->pExcare, 0, 0, 0 );
        p->pCare = Abc_NtkToDar( pTemp, 0 );
        Abc_NtkDelete( pTemp );
        p->vSuppsInv = Aig_ManSupportsInverse( p->pCare );
    }
//    if ( pPars->fVerbose )
    {
        if ( p->pCare != NULL )
            printf( "Performing optimization with %d external care clauses.\n", Aig_ManPoNum(p->pCare) );
//        else
//            printf( "Performing optimization without constraints.\n" );
    }
    if ( !pPars->fResub )
    {
        pDecPars->nVarsMax = nFaninMax;
        pDecPars->fVerbose = pPars->fVerbose;
        p->vTruth = Vec_IntAlloc( 0 );
        p->pManDec = Bdc_ManAlloc( pDecPars );
    }

    // label the register outputs
    if ( p->pCare )
    {
        Abc_NtkForEachCi( pNtk, pObj, i )
            pObj->pData = (void *)i;
    }
 
    // compute levels
    Abc_NtkLevel( pNtk );
    Abc_NtkStartReverseLevels( pNtk, pPars->nGrowthLevel );

    // compute don't-cares for each node
    nNodes = 0;
    p->nTotalNodesBeg = nTotalNodesBeg;
    p->nTotalEdgesBeg = nTotalEdgesBeg;
    if ( pPars->fResub )
    {
        pProgress = Extra_ProgressBarStart( stdout, Abc_NtkObjNumMax(pNtk) );
        Abc_NtkForEachNode( pNtk, pObj, i )
        {
            if ( p->pPars->nDepthMax && (int)pObj->Level > p->pPars->nDepthMax )
                continue;
            if ( Abc_ObjFaninNum(pObj) < 2 || Abc_ObjFaninNum(pObj) > nFaninMax )
                continue;
            if ( !p->pPars->fVeryVerbose )
                Extra_ProgressBarUpdate( pProgress, i, NULL );
            if ( pPars->fResub )
                Abc_NtkMfsResub( p, pObj );
            else
                Abc_NtkMfsNode( p, pObj );
        }
        Extra_ProgressBarStop( pProgress );
    }
    else
    {
        pProgress = Extra_ProgressBarStart( stdout, Abc_NtkNodeNum(pNtk) );
        vLevels = Abc_NtkLevelize( pNtk );
        Vec_VecForEachLevelStart( vLevels, vNodes, k, 1 )
        {
            if ( !p->pPars->fVeryVerbose )
                Extra_ProgressBarUpdate( pProgress, nNodes, NULL );
            p->nNodesGainedLevel = 0;
            p->nTotConfLevel = 0;
            p->nTimeOutsLevel = 0;
            clk2 = clock();
            Vec_PtrForEachEntry( vNodes, pObj, i )
            {
                if ( p->pPars->nDepthMax && (int)pObj->Level > p->pPars->nDepthMax )
                    break;
                if ( Abc_ObjFaninNum(pObj) < 2 || Abc_ObjFaninNum(pObj) > nFaninMax )
                    continue;
                if ( pPars->fResub )
                    Abc_NtkMfsResub( p, pObj );
                else 
                    Abc_NtkMfsNode( p, pObj );
            }
            nNodes += Vec_PtrSize(vNodes);
            if ( pPars->fVerbose )
            {
            printf( "Lev = %2d. Node = %5d. Ave gain = %5.2f. Ave conf = %5.2f. T/o = %6.2f %%  ", 
                k, Vec_PtrSize(vNodes),
                1.0*p->nNodesGainedLevel/Vec_PtrSize(vNodes),
                1.0*p->nTotConfLevel/Vec_PtrSize(vNodes),
                100.0*p->nTimeOutsLevel/Vec_PtrSize(vNodes) );
            PRT( "Time", clock() - clk2 );
            }
        }
        Extra_ProgressBarStop( pProgress );
        Vec_VecFree( vLevels );
    }
    Abc_NtkStopReverseLevels( pNtk );

    // perform the sweeping
    if ( !pPars->fResub )
    {
        extern void Abc_NtkBidecResyn( Abc_Ntk_t * pNtk, int fVerbose );
//        Abc_NtkSweep( pNtk, 0 );
//        Abc_NtkBidecResyn( pNtk, 0 );
    }

    p->nTotalNodesEnd = Abc_NtkNodeNum(pNtk);
    p->nTotalEdgesEnd = Abc_NtkGetTotalFanins(pNtk);

    // undo labesl
    if ( p->pCare )
    {
        Abc_NtkForEachCi( pNtk, pObj, i )
            pObj->pData = NULL;
    }

    // free the manager
    p->timeTotal = clock() - clk;
    Mfs_ManStop( p );
    return 1;
}

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////


