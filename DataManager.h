/** @file DataManager.h
 The DataManager holds data that TreePieces need access to, but not
 their own copy.
 @author Graeme Lufkin (gwl@u.washington.edu)
*/

#ifndef DATAMANAGER_H
#define DATAMANAGER_H

#include <vector>
#include <map>
#include <string>
#include "GenericTreeNode.h"
#include "ParallelGravity.decl.h"

#ifdef CUDA

struct TreePieceDescriptor{
	TreePiece *tp;
	int index;

	TreePieceDescriptor(){}
	TreePieceDescriptor(TreePiece *tp_, int index_){
		tp = tp_;
		index = index_;
	}
};

struct UpdateParticlesStruct{
  CkCallback *cb;
  DataManager *dm;
  VariablePartData *buf;
  int size;
};


// store pointers to flattened buffers if !gpuFree 
struct PendingBuffers {
  CkVec<CudaMultipoleMoments> *moments;
  CkVec<CompactPartData> *particles;
  int chunk;
  //CudaMultipoleMoments *moments;
  //int nMoments;
  //CompactPartData *particles;
  //int nParticles;
};

#endif

/** The DataManager is used to store information that all TreePieces will need,
 but will not modify.  The first example is the list of splitter keys and the
 responsible chare for each interval.  This data is given to the DataManager by
 the Sorter.  The DataManager then instructs all the TreePieces on its node
 to evaluate the boundary keys.
 */
class DataManager : public CBase_DataManager {
	friend class TreePiece;

	/// The array of TreePieces I hold data for.
	CProxy_TreePiece treePieces;

protected:

	/// The array of splitter keys for the sort.
	std::vector<SFC::Key> boundaryKeys;

	/// An array identifying which chare is responsible for each interval of keys.
	std::vector<int> responsibleIndex;

	/// An array with how many particles are held by each TreePiece when sorted.
	std::vector<int> particleCounts;

	/// An array with a list of the first and last particle of each TreePiece
	SFC::Key * splitters;
	/// The size of the array splitters
	int numSplitters;

	/// A list of roots of the TreePieces in this node
	CkVec<Tree::GenericTreeNode*> registeredChares;
#ifdef CUDA
	// holds chare array indices of registered treepieces
	CkVec<TreePieceDescriptor> registeredTreePieces;
	//CkVec<int> registeredTreePieceIndices;
        int cumNumReplicatedNodes;
        int treePiecesDone;
        int savedChunk;
        int treePiecesDonePrefetch;
        int treePiecesDoneLocalComputation;
        // XXX - assumes that only one chunk can be on the gpu
        // at a given time
        int treePiecesDoneRemoteChunkComputation;
        int treePiecesWantParticlesBack;
        int savedNumTotalParticles;
        int savedNumTotalNodes;
        // keeps track of buckets of particles that were
        // received during the prefetch and which were subsequently
        // shipped off to the gpu - XXX
        // not including cached particles in postfrefetch entities shipped to gpu
        // since it is hard to count their number given just the pointer to the cache entry
        // * either do not concern yourself with cached particles
        // * or for each entry, get key, find bucket node in CM, DM or TPs and get number
        // for now, the former

        //std::map<NodeKey, int> cachedPartsOnGpu;
        // local particles that have been copied to the gpu
        //std::map<NodeKey, int> localPartsOnGpu;

        //std::map<Tree::NodeKey, GenericTreeNode *> missedNodesOnGpu;
        //std::map<Tree::NodeKey, ExternalGravityParticle *> missedPartsOnGpu;

        // can the gpu accept a chunk of remote particles/nodes?
        bool gpuFree;
        // queue that stores all pending chunk transfers
        CkQ<PendingBuffers *> pendingChunkTransferQ;

        // last remote chunk's size in moments and particles
        int lastChunkMoments;
        int lastChunkParticles;

#ifdef CUDA_INSTRUMENT_WRS
        int activeRung;
        int treePiecesDoneInitInstrumentation;
#endif
#endif
	/// The root of the combined trees
	Tree::GenericTreeNode * root;
	/// The lookup table for nodes created by the DataManager to combine trees
	Tree::NodeLookupType nodeLookupTable;

	/// Number of chunks in which the tree was splitted during last combine operation
	int oldNumChunks;
	/// Nodes currently used as roots for remote computation
	Tree::NodeKey *chunkRoots;
        /// Lookup table for the chunkRoots
        Tree::NodeLookupType chunkRootTable;

public:

	/* 
	 ** Cooling 
	 */
	COOL *Cool;

	DataManager(const CkArrayID& treePieceID);
	DataManager(CkMigrateMessage *);

#ifdef CUDA
        //void serializeNodes(GenericTreeNode *start, CudaMultipoleMoments *&postPrefetchMoments, CompactPartData *&postPrefetchParticles);
		//void serializeNodes(GenericTreeNode *start);
        void donePrefetch(int chunk); // serialize remote chunk wrapper
        void serializeLocalTree();

        // actual serialization methods
        PendingBuffers *serializeRemoteChunk(GenericTreeNode *);
	void serializeLocal(GenericTreeNode *);
        void freeLocalTreeMemory();
        void freeRemoteChunkMemory(int chunk);
        void transferParticleVarsBack();
        void updateParticles(UpdateParticlesStruct *data);
        void initiateNextChunkTransfer();
#ifdef CUDA_INSTRUMENT_WRS
        int initInstrumentation();
#endif
        DataManager(){} 
#endif
        void clearInstrument(CkCallback &cb);

private:
        void init();

public:

	~DataManager() {
	    CoolFinalize(Cool);
	    }

	/// Collect the boundaries of all TreePieces, and trigger the real treebuild
	void collectSplitters(CkReductionMsg* m);
	/// Called by ORB Sorter, save the list of which TreePiece is
	/// responsible for which interval.
	void acceptResponsibleIndex(const int* responsible, const int n,
				    const CkCallback& cb);
	/// Called by the Sorter, I save these final keys and the list
	/// of which TreePiece is responsible for which interval
	void acceptFinalKeys(const SFC::Key* keys, const int* responsible, unsigned int* bins, const int n, const CkCallback& cb);
	void pup(PUP::er& p);

	// Functions used to create a tree inside the DataManager comprising
	// all the trees in the TreePieces in the local node
private:
	Tree::GenericTreeNode *buildProcessorTree(int n, Tree::GenericTreeNode **gtn);
	int createLookupRoots(Tree::GenericTreeNode *node, Tree::NodeKey *keys);
public:

#ifdef CUDA
    //XXX - coercing arrayindex to int in last arg      
    void notifyPresence(Tree::GenericTreeNode *root, TreePiece *tp, int index);
#else
	void notifyPresence(Tree::GenericTreeNode *root);
#endif
    void combineLocalTrees(CkReductionMsg *msg);
    void getChunks(int &num, Tree::NodeKey *&roots);
    inline Tree::GenericTreeNode *chunkRootToNode(const Tree::NodeKey k) {
      NodeLookupType::iterator iter = chunkRootTable.find(k);
      if (iter != chunkRootTable.end()) return iter->second;
      else return NULL;
    }
    inline Tree::GenericTreeNode *getRoot() { return root; }
    void initCooling(double dGmPerCcUnit, double dComovingGmPerCcUnit,
		     double dErgPerGmUnit, double dSecUnit, double dKpcUnit,
		     COOLPARAM inParam, const CkCallback& cb);
    void dmCoolTableRead(double *dTableData, int nData, const CkCallback& cb);
    void CoolingSetTime(double z, // redshift
			double dTime, // Time
			const CkCallback& cb);
};

#endif //DATAMANAGER_H