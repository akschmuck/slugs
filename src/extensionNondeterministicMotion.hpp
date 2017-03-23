#ifndef __EXTENSION_NONTERMINISTIC_MOTION_HPP
#define __EXTENSION_NONTERMINISTIC_MOTION_HPP

#include "gr1context.hpp"

template<class T, bool initSpecialRoboticsSemantics, bool addSelfLoopLivenessAssumptions> class XNonDeterministicMotion : public T {
protected:

    // Inherited stuff used
    using T::mgr;
    using T::initEnv;
    using T::initSys;
    using T::safetyEnv;
    using T::safetySys;
    using T::lineNumberCurrentlyRead;
    using T::addVariable;
    using T::livenessGuarantees;
    using T::livenessAssumptions;
    using T::variableNames;
    using T::variables;
    using T::variableTypes;
    using T::varCubePost;
    using T::varCubePre;
    using T::varVectorPre;
    using T::varVectorPost;
    using T::strategyDumpingData;
    using T::doesVariableInheritType;
    using T::varCubePostInput;
    using T::winningPositions;
    using T::varCubePreInput;
    using T::varCubePreOutput;
    using T::realizable;
    using T::preVars;
    using T::postVars;
    using T::determinize;
    using T::computeVariableInformation;

    // Own variables local to this plugin
    BF robotBDD;
    SlugsVectorOfVarBFs preMotionStateVars{PreMotionState,this};
    SlugsVectorOfVarBFs postMotionStateVars{PostMotionState,this};
    SlugsVectorOfVarBFs postMotionControlVars{PostMotionControlOutput,this};
    SlugsVarCube varCubePreMotionState{PreMotionState,this};
    SlugsVarCube varCubePostMotionState{PostMotionState,this};
    SlugsVarCube varCubePostControllerOutput{PostMotionControlOutput,this};

public:
    static GR1Context* makeInstance(std::list<std::string> &filenames) {
        return new XNonDeterministicMotion<T,initSpecialRoboticsSemantics,addSelfLoopLivenessAssumptions>(filenames);
    }

    XNonDeterministicMotion<T,initSpecialRoboticsSemantics,addSelfLoopLivenessAssumptions>(std::list<std::string> &filenames): T(filenames) {}

    /**
     * @brief Recurse internal function to parse a Boolean formula from a line in the input file
     * @param is the input stream from which the tokens in the line are read
     * @param allowedTypes a list of allowed variable types - this allows to check that assumptions do not refer to 'next' output values.
     * @param readBDDs a list of bdds that can be accessed by names
     * @return a BF that represents the transition constraint read from the line
     */
    BF parseBooleanFormulaRecurseEx(std::istringstream &is,std::set<VariableType> &allowedTypes, std::vector<BF> &memory,const std::map<std::string,BF> &readBDDs) {
        std::string operation = "";
        is >> operation;
        if (operation=="") {
            SlugsException e(false);
            e << "Error reading line " << lineNumberCurrentlyRead << ". Premature end of line.";
            throw e;
        }
        if (operation=="|") return parseBooleanFormulaRecurseEx(is,allowedTypes,memory,readBDDs) | parseBooleanFormulaRecurseEx(is,allowedTypes,memory,readBDDs);
        if (operation=="^") return parseBooleanFormulaRecurseEx(is,allowedTypes,memory,readBDDs) ^ parseBooleanFormulaRecurseEx(is,allowedTypes,memory,readBDDs);
        if (operation=="&") return parseBooleanFormulaRecurseEx(is,allowedTypes,memory,readBDDs) & parseBooleanFormulaRecurseEx(is,allowedTypes,memory,readBDDs);
        if (operation=="!") return !parseBooleanFormulaRecurseEx(is,allowedTypes,memory,readBDDs);
        if (operation=="1") return mgr.constantTrue();
        if (operation=="0") return mgr.constantFalse();

        // Memory Functionality - Create Buffer
        if (operation=="$") {
            unsigned nofElements;
            is >> nofElements;
            if (is.fail()) {
                SlugsException e(false);
                e << "Error reading line " << lineNumberCurrentlyRead << ". Expected number of memory elements.";
                throw e;
            }
            std::vector<BF> memoryNew(nofElements);
            for (unsigned int i=0;i<nofElements;i++) {
                memoryNew[i] = parseBooleanFormulaRecurseEx(is,allowedTypes,memoryNew,readBDDs);
            }
            return memoryNew[nofElements-1];
        }

        // Memory Functionality - Recall from Buffer
        if (operation=="?") {
            unsigned int element;
            is >> element;
            if (is.fail()) {
                SlugsException e(false);
                e << "Error reading line " << lineNumberCurrentlyRead << ". Expected number after memory recall operator '?'.";
                throw e;
            }
            if (element>=memory.size()) {
                SlugsException e(false);
                e << "Error reading line " << lineNumberCurrentlyRead << ". Trying to recall a memory element that has not been stored (yet).";
                throw e;
            }
            return memory[element];
        }

        // Is BDD?
        auto it = readBDDs.find(operation);
        if (it!=readBDDs.end()) {
            return it->second;
        }

        // Has to be a variable!
        for (unsigned int i=0;i<variableNames.size();i++) {
            if (variableNames[i]==operation) {
                if (allowedTypes.count(variableTypes[i])==0) {
                    SlugsException e(false);
                    e << "Error reading line " << lineNumberCurrentlyRead << ". The variable " << operation << " is not allowed for this type of expression.";
                    throw e;
                }
                return variables[i];
            }
        }
        SlugsException e(false);
        e << "Error reading line " << lineNumberCurrentlyRead << ". The variable " << operation << " has not been found.";
        throw e;
    }

    /**
     * @brief Internal function for parsing a Boolean formula from a line in the input file - calls the recursive function to do all the work.
     * @param currentLine the line to parse
     * @param allowedTypes a list of allowed variable types - this allows to check that assumptions do not refer to 'next' output values.
     * @param readBDDs a list of bdds that can be accessed by names
     * @return a BF that represents the transition constraint read from the line
     */
    BF parseBooleanFormulaEx(std::string currentLine, std::set<VariableType> &allowedTypes, const std::map<std::string,BF> &readBDDs) {

        std::istringstream is(currentLine);

        std::vector<BF> memory;
        BF result = parseBooleanFormulaRecurseEx(is,allowedTypes,memory,readBDDs);
        assert(memory.size()==0);
        std::string nextPart = "";
        is >> nextPart;
        if (nextPart=="") return result;

        SlugsException e(false);
        e << "Error reading line " << lineNumberCurrentlyRead << ". There are stray characters: '" << nextPart << "'";
        throw e;
    }




    /**
     * @brief init - Read input file(s)
     * @param filenames
     */
    void init(std::list<std::string> &filenames) {

        if (filenames.size()==0) {
            throw "Error: Cannot load SLUGS input file - there has been no input file name given!";
        }

        if (filenames.size()<2) {
            throw "Error: At least two file names are needed when using the supplied options!";
        }

        std::string specFileName = *(filenames.begin());
        filenames.pop_front();

        std::ifstream inFile(specFileName.c_str());
        if (inFile.fail()) throw "Error: Cannot open input file";

        // Prepare safety and initialization constraints
        initEnv = mgr.constantTrue();
        initSys = mgr.constantTrue();
        safetyEnv = mgr.constantTrue();
        safetySys = mgr.constantTrue();

        // The readmode variable stores in which chapter of the input file we are
        int readMode = -1;
        std::string currentLine;
        lineNumberCurrentlyRead = 0;
        std::map<std::string,BF> readBDDs;
        while (std::getline(inFile,currentLine)) {
            lineNumberCurrentlyRead++;
            boost::trim(currentLine);
            if ((currentLine.length()>0) && (currentLine[0]!='#')) {
                if (currentLine[0]=='[') {
                    if (currentLine=="[INPUT]") {
                        readMode = 0;
                    } else if (currentLine=="[MOTION STATE OUTPUT]") {
                        readMode = 1;
                    } else if (currentLine=="[MOTION CONTROLLER OUTPUT]") {
                        readMode = 2;
                    } else if (currentLine=="[OTHER OUTPUT]") {
                        readMode = 3;
                    } else if (currentLine=="[ENV_INIT]") {
                        readMode = 4;
                    } else if (currentLine=="[SYS_INIT]") {
                        readMode = 5;
                    } else if (currentLine=="[ENV_TRANS]") {
                        readMode = 6;
                    } else if (currentLine=="[SYS_TRANS]") {
                        readMode = 7;
                    } else if (currentLine=="[ENV_LIVENESS]") {
                        readMode = 8;
                    } else if (currentLine=="[SYS_LIVENESS]") {
                        readMode = 9;
                    } else if (currentLine=="[HELPER_BDDS]") {
                        readMode = 10;
                    } else {
                        std::cerr << "Sorry. Didn't recognize category " << currentLine << "\n";
                        throw "Aborted.";
                    }
                } else {
                    if (readMode==0) {
                        variables.push_back(mgr.newVariable());
                        variableNames.push_back(currentLine);
                        variableTypes.push_back(PreInput);
                        variables.push_back(mgr.newVariable());
                        variableNames.push_back(currentLine+"'");
                        variableTypes.push_back(PostInput);
                    } else if (readMode==1) {
                        variables.push_back(mgr.newVariable());
                        variableNames.push_back(currentLine);
                        variableTypes.push_back(PreMotionState);
                        variables.push_back(mgr.newVariable());
                        variableNames.push_back(currentLine+"'");
                        variableTypes.push_back(PostMotionState);
                    } else if (readMode==2) {
                        variables.push_back(mgr.newVariable());
                        variableNames.push_back(currentLine);
                        variableTypes.push_back(PreMotionControlOutput);
                        variables.push_back(mgr.newVariable());
                        variableNames.push_back(currentLine+"'");
                        variableTypes.push_back(PostMotionControlOutput);
                    } else if (readMode==3) {
                        variables.push_back(mgr.newVariable());
                        variableNames.push_back(currentLine);
                        variableTypes.push_back(PreOtherOutput);
                        variables.push_back(mgr.newVariable());
                        variableNames.push_back(currentLine+"'");
                        variableTypes.push_back(PostOtherOutput);
                    } else if (readMode==4) {
                        std::set<VariableType> allowedTypes;
                        allowedTypes.insert(PreInput);
                        // allowedTypes.insert(PreMotionState);
                        // allowedTypes.insert(PreMotionControlOutput); -> Is not taken into account
                        // allowedTypes.insert(PreOtherOutput);
                        initEnv &= parseBooleanFormulaEx(currentLine,allowedTypes,readBDDs);
                    } else if (readMode==5) {
                        std::set<VariableType> allowedTypes;
                        allowedTypes.insert(PreInput);
                        allowedTypes.insert(PreMotionState);
                        allowedTypes.insert(PreMotionControlOutput);
                        allowedTypes.insert(PreOtherOutput);
                        initSys &= parseBooleanFormulaEx(currentLine,allowedTypes,readBDDs);
                    } else if (readMode==6) {
                        std::set<VariableType> allowedTypes;
                        allowedTypes.insert(PreInput);
                        allowedTypes.insert(PreMotionState);
                         // allowedTypes.insert(PreMotionControlOutput); -> Is not taken into account
                        allowedTypes.insert(PreOtherOutput);
                        allowedTypes.insert(PostInput);
                        //allowedTypes.insert(PostMotionState);
                        //allowedTypes.insert(PostOtherOutput);
                        safetyEnv &= parseBooleanFormulaEx(currentLine,allowedTypes,readBDDs);
                    } else if (readMode==7) {
                        std::set<VariableType> allowedTypes;
                        allowedTypes.insert(PreInput);
                        allowedTypes.insert(PreMotionState);
                        allowedTypes.insert(PostMotionControlOutput);
                        allowedTypes.insert(PreOtherOutput);
                        allowedTypes.insert(PostInput);
                        allowedTypes.insert(PostMotionState);
                        allowedTypes.insert(PostOtherOutput);
                        safetySys &= parseBooleanFormulaEx(currentLine,allowedTypes,readBDDs);
                    } else if (readMode==8) {
                        std::set<VariableType> allowedTypes;
                        allowedTypes.insert(PreInput);
                        allowedTypes.insert(PreMotionState);
                         // allowedTypes.insert(PreMotionControlOutput); -> Is not taken into account
                        allowedTypes.insert(PreOtherOutput);
                        allowedTypes.insert(PostInput);
                        livenessAssumptions.push_back(parseBooleanFormulaEx(currentLine,allowedTypes,readBDDs));
                    } else if (readMode==9) {
                        std::set<VariableType> allowedTypes;
                        allowedTypes.insert(PreInput);
                        allowedTypes.insert(PreMotionState);
                        allowedTypes.insert(PostMotionControlOutput);
                        allowedTypes.insert(PreOtherOutput);
                        allowedTypes.insert(PostInput);
                        allowedTypes.insert(PostMotionState);
                        allowedTypes.insert(PostOtherOutput);
                        livenessGuarantees.push_back(parseBooleanFormulaEx(currentLine,allowedTypes,readBDDs));
                    } else if (readMode==10) {
                        if (currentLine.find(" ")==std::string::npos) {
                            std::cerr << "Error with line " << lineNumberCurrentlyRead << "!";
                            throw "There must be at least one space in a BDD definition line.";
                        }
                        std::string bddName = currentLine.substr(0,currentLine.find(" "));
                        std::string bddFile = currentLine.substr(currentLine.find(" ")+1,std::string::npos);
                        std::vector<BF> varsBDDread;
                        // Invert all order to get the least significant bit first
                        for (int i=variables.size()-1;i>=0;i--) {
                            varsBDDread.push_back(variables[i]);
                        }
                        /*for (int i=variables.size()-1;i>=0;i--) {
                            if (variableTypes[i]==PostMotionControlOutput)
                            varsBDDread.push_back(variables[i]);
                        }
                        for (int i=variables.size()-1;i>=0;i--) {
                            if (variableTypes[i]==PostMotionState)
                            varsBDDread.push_back(variables[i]);
                        }*/
                        BF newBDD = mgr.readBDDFromFile(bddFile.c_str(),varsBDDread);
                        if (readBDDs.count(bddName)>0) {
                            std::cerr << "Error with line " << lineNumberCurrentlyRead << "!";
                            throw "A BDD with that name already exists.";
                        }
                        readBDDs[bddName] = newBDD;
                        computeVariableInformation(); // Becuase varVectors are used below.
                        readBDDs[bddName+"'"] = newBDD.SwapVariables(varVectorPre,varVectorPost);
                    } else {
                        std::cerr << "Error with line " << lineNumberCurrentlyRead << "!";
                        throw "Found a line in the specification file that has no proper categorial context.";
                    }
                }
            }
        }

        std::vector<BF> varsBDDread;
        // Invert all order to get the least significant bit first
        for (int i=variables.size()-1;i>=0;i--) {
            if (variableTypes[i]==PreMotionState)
            varsBDDread.push_back(variables[i]);
        }
        for (int i=variables.size()-1;i>=0;i--) {
            if (variableTypes[i]==PostMotionControlOutput)
            varsBDDread.push_back(variables[i]);
        }
        for (int i=variables.size()-1;i>=0;i--) {
            if (variableTypes[i]==PostMotionState)
            varsBDDread.push_back(variables[i]);
        }
        std::cerr << "Numer of bits that we expect the robot abstraction BDD to have: " << varsBDDread.size() << std::endl;
        mgr.setAutomaticOptimisation(false);
        computeVariableInformation();

        robotBDD = mgr.constantFalse();
        if (filenames.size()==0) throw "Error: No BDD file given.";
        std::string allFilenames = *(filenames.begin());
        filenames.pop_front();

        while (allFilenames.size()>0) {
            std::string frontFilename;
            size_t colonIndex = allFilenames.find(":");
            if (colonIndex==std::string::npos) {
                frontFilename = allFilenames;
                allFilenames = "";
            } else {
                frontFilename = allFilenames.substr(0,colonIndex);
                allFilenames = allFilenames.substr(colonIndex+1,std::string::npos);
            }

            BF newPart = mgr.readBDDFromFile(frontFilename.c_str(),variables,variableNames);
            if (!((newPart.ExistAbstract(varCubePostMotionState) & robotBDD.ExistAbstract(varCubePostMotionState)).isFalse())) {
                std::cerr << "Warning: Partitioned Robot BDDs are not split up correctly\n";
            }
            robotBDD |= newPart;
        }

        if (robotBDD.isConstant()) throw "Failed to read Robot Abstraction.";
        std::cerr << "Continuing computation...\n";

        // Finally, add the liveness assumption that if we are executing an action that *can* lead
        // to motion, we do so.
        computeVariableInformation();


        // Compute reachability fixpoint
        /*BFFixedPoint nu2(mgr.constantTrue());

        {
            unsigned int outerIteration = 0;

            // Iterate until we have found a fixed point
            for (;!nu2.isFixedPointReached();) {

                outerIteration++;
                BF newBackReach = robotBDD & nu2.getValue().SwapVariables(varVectorPre,varVectorPost);
                newBackReach = newBackReach.ExistAbstract(varCubePost);
                nu2.update(newBackReach);

            }
        }

        std::map<std::string,BF> inspectorContext;
        inspectorContext["onePlayerStable"] = nu2.getValue();

        // From where are we unable to change direction?
        BF curHeadingChange = mgr.constantFalse();
        for (unsigned int i=0;i<variables.size();i++) {
            if (doesVariableInheritType(i,Pre)) {
                if (variableNames[i].substr(0,10)=="curHeading") {
                    curHeadingChange |= variables[i] ^ variables[i+1];
                }
            }
        }

        BF actionsWithoutChange = (robotBDD & !curHeadingChange).ExistAbstract(varCubePostMotionState);
        BF actionsWithOnlyChanges = (!actionsWithoutChange & robotBDD.ExistAbstract(varCubePostMotionState));
        BF statesThatAllowToEnforceChanges = actionsWithOnlyChanges.ExistAbstract(varCubePost);
        BF cannotChange = nu2.getValue() & !statesThatAllowToEnforceChanges;
        inspectorContext["cannotChange"] = cannotChange;

        // bddInspector(inspectorContext);

        // Paint robot BDD
        //std::cerr << "DOTTing Robot BDD\n";
        BF toDraw = robotBDD & (!variables[0])
                             & (variables[2])
                             & (!variables[4])
                             & (!variables[6])
                             & (!variables[8])
                             & (!variables[10])
                             & (!variables[12])
                             & (!variables[14]) & variables[16] & variables[18] & !variables[20]
                             & (!variables[22]) & variables[24] & variables[26] & !variables[28]
                            // Motion Control
                             & (!variables[31]) & !variables[33] & !variables[35] & !variables[37] & !variables[39] & !variables[41] & !variables[43] & !variables[45] & !variables[47];


        std::cerr << "Restricting: " << variableNames[0] << " to " << variableNames[28] << " to " << variableNames[47];
        //std::cerr << "Restricting to 3: " << variableNames[12] << "," << variableNames[14] << "," << variableNames[16] << std::endl;
        BF_newDumpDot(*this,robotBDD,NULL,"/tmp/robotBDD.dot");
        //std::cerr << "DOTTing Robot BDD done!\n";


        // Compute non-crashing game
        BFFixedPoint nonCrash(mgr.constantTrue());
        std::vector<BF> prefixPoints;
        {
            unsigned int outerIteration = 0;
            BF robotBDDAvailableActions = robotBDD.ExistAbstract(varCubePostMotionState);

            // Iterate until we have found a fixed point
            for (;!nonCrash.isFixedPointReached();) {

                outerIteration++;
                BF badActions = robotBDD & !(nonCrash.getValue().SwapVariables(varVectorPre,varVectorPost));
                badActions = badActions.ExistAbstract(varCubePostMotionState);
                BF goodStates = (robotBDDAvailableActions & !badActions).ExistAbstract(varCubePost);

                // Good states debugging hack
                for (unsigned int i=0;i<variables.size();i++) {
                    if (doesVariableInheritType(i,Pre)) {
                        if (variableNames[i].substr(0,7)=="xbit@0.") {
                            goodStates = goodStates.UnivAbstractSingleVar(variables[i]);
                            std::cerr << "(FoundXVAR)";
                        } else if (variableNames[i].substr(0,7)=="ybit@0.") {
                            goodStates = goodStates.UnivAbstractSingleVar(variables[i]);
                            std::cerr << "(FoundYVAR)";
                        }
                    }
                }

                nonCrash.update(goodStates);
                if (!(goodStates.isFalse())) {
                    prefixPoints.push_back(goodStates);
                } else {
                    std::cerr << "---UNCONTROLLABLE!---";
                }

                // Dumping dot
                std::ostringstream filename;
                filename << "/tmp/prefix" << outerIteration << ".dot";
                BF_newDumpDot(*this,nonCrash.getValue(),NULL,filename.str());

            }
        }

        // Generate a trace
        std::cerr << "\nTrace:\n";
        BF currentState = determinize(prefixPoints.back(),preVars);
        int outerIteration  = 0;
        BF robotBDDAvailableActions = robotBDD.ExistAbstract(varCubePostMotionState);

        for (int i = prefixPoints.size()-1;i>0;i--) {

            outerIteration++;

            std::ostringstream filename;
            filename << "/tmp/currState" << outerIteration << ".dot";
            BF_newDumpDot(*this,currentState,NULL,filename.str());

            printPreState(currentState);

            BF nextTrans = (robotBDD & currentState).ExistAbstract(varCubePre);

            BF locallyAvailableActions = nextTrans.ExistAbstract(varCubePostMotionState);

            nextTrans = !nextTrans | prefixPoints[i-1].SwapVariables(varVectorPre,varVectorPost);

            std::ostringstream filenameC;
            filenameC << "/tmp/nextTrans" << outerIteration << ".dot";
            BF_newDumpDot(*this,nextTrans,"Post",filenameC.str());

            BF selectedAction = determinize((locallyAvailableActions & nextTrans).UnivAbstract(varCubePostMotionState),postMotionControlVars);
            if (selectedAction.isFalse()) std::cerr << "Found no transition.";

            std::ostringstream filenameB;
            filenameB << "/tmp/selectedAction" << outerIteration << ".dot";
            BF_newDumpDot(*this,selectedAction,NULL,filenameB.str());


            BF nextState = determinize((robotBDD & currentState & selectedAction & !prefixPoints[i].SwapVariables(varVectorPre,varVectorPost)).ExistAbstract(varCubePre),postVars).SwapVariables(varVectorPre,varVectorPost);
            currentState = nextState;
        }
        printPreState(currentState);
        */

        // Add liveness assumption
        if (addSelfLoopLivenessAssumptions)
            addAutomaticallyGeneratedLivenessAssumption();

    }

    void checkRealizability() {

        // Compute first which moves by the robot are actually allowed.
        BF robotAllowedMoves = robotBDD.ExistAbstract(varCubePostMotionState);

        // The greatest fixed point - called "Z" in the GR(1) synthesis paper
        BFFixedPoint nu2(mgr.constantTrue());

        unsigned int outerIteration = 0;

        // Iterate until we have found a fixed point
        for (;!nu2.isFixedPointReached();) {

            outerIteration++;

            std::ostringstream os;
            os << "/tmp/pfp" << outerIteration << ".dot";
            BF_newDumpDot(*this,nu2.getValue(),NULL,os.str());

            // To extract a strategy in case of realizability, we need to store a sequence of 'preferred' transitions in the
            // game structure. These preferred transitions only need to be computed during the last execution of the outermost
            // greatest fixed point. Since we don't know which one is the last one, we store them in every iteration,
            // so that after the last iteration, we obtained the necessary data. Before any new iteration, we need to
            // clear the old data, though.
            strategyDumpingData.clear();

            // Iterate over all of the liveness guarantees. Put the results into the variable 'nextContraintsForGoals' for every
            // goal. Then, after we have iterated over the goals, we can update nu2.
            BF nextContraintsForGoals = mgr.constantTrue();
            for (unsigned int j=0;j<livenessGuarantees.size();j++) {

                // Start computing the transitions that lead closer to the goal and lead to a position that is not yet known to be losing.
                // Start with the ones that actually represent reaching the goal (which is a transition in this implementation as we can have
                // nexts in the goal descriptions).
                BF livetransitions = livenessGuarantees[j] & (nu2.getValue().SwapVariables(varVectorPre,varVectorPost));
                //BF_newDumpDot(*this,livetransitions,NULL,"/tmp/liveTransitions.dot");

                // Compute the middle least-fixed point (called 'Y' in the GR(1) paper)
                BFFixedPoint mu1(mgr.constantFalse());
                for (;!mu1.isFixedPointReached();) {

                    // Update the set of transitions that lead closer to the goal.
                    livetransitions |= mu1.getValue().SwapVariables(varVectorPre,varVectorPost);

                    // Iterate over the liveness assumptions. Store the positions that are found to be winning for *any*
                    // of them into the variable 'goodForAnyLivenessAssumption'.
                    BF goodForAnyLivenessAssumption = mu1.getValue();
                    for (unsigned int i=0;i<livenessAssumptions.size();i++) {

                        // Prepare the variable 'foundPaths' that contains the transitions that stay within the inner-most
                        // greatest fixed point or get closer to the goal. Only used for strategy extraction
                        BF foundPaths = mgr.constantTrue();

                        // Inner-most greatest fixed point. The corresponding variable in the paper would be 'X'.
                        BFFixedPoint nu0(mgr.constantTrue());
                        for (;!nu0.isFixedPointReached();) {

                            // Compute a set of paths that are safe to take - used for the enforceable predecessor operator ('cox')
                            foundPaths = livetransitions | (nu0.getValue().SwapVariables(varVectorPre,varVectorPost) & !(livenessAssumptions[i]));
                            foundPaths &= safetySys;
                            //BF_newDumpDot(*this,foundPaths,NULL,"/tmp/foundPathsPreRobot.dot");
                            foundPaths = robotAllowedMoves & robotBDD.Implies(foundPaths).UnivAbstract(varCubePostMotionState);
                            //BF_newDumpDot(*this,foundPaths,NULL,"/tmp/foundPathsPostRobot.dot");

                            // Update the inner-most fixed point with the result of applying the enforcable predecessor operator
                            nu0.update(safetyEnv.Implies(foundPaths).ExistAbstract(varCubePostControllerOutput).UnivAbstract(varCubePostInput));
                        }

                        // Update the set of positions that are winning for some liveness assumption
                        goodForAnyLivenessAssumption |= nu0.getValue();

                        // Dump the paths that we just wound into 'strategyDumpingData' - store the current goal long
                        // with the BDD
                        strategyDumpingData.push_back(std::pair<unsigned int,BF>(j,foundPaths & robotBDD));
                    }

                    // Update the moddle fixed point
                    mu1.update(goodForAnyLivenessAssumption);
                }

                // Update the set of positions that are winning for any goal for the outermost fixed point
                nextContraintsForGoals &= mu1.getValue();
            }

            // Update the outer-most fixed point
            std::ostringstream diffFileDot;
            diffFileDot << "/tmp/diff" << outerIteration << ".dot";
            BF diff = nu2.getValue() & !nextContraintsForGoals;
            diff = determinize(diff,preVars);
            BF_newDumpDot(*this,diff,"Pre",diffFileDot.str());

            nu2.update(nextContraintsForGoals);

        }

        // We found the set of winning positions
        winningPositions = nu2.getValue();
        BF_newDumpDot(*this,winningPositions,NULL,"/tmp/winningPositions.dot");

        // Check if for every possible environment initial position the system has a good system initial position
        BF result;
        if (initSpecialRoboticsSemantics) {
            if (!initSys.isTrue()) std::cerr << "Warning: Initialisation guarantees have been given although these are ignored in semantics-for-robotics mode! \n";
            result = initEnv.Implies(winningPositions.UnivAbstract(varCubePreOutput)).UnivAbstract(varCubePreInput);

        } else {
            result = initEnv.Implies((winningPositions & initSys).ExistAbstract(varCubePreOutput)).UnivAbstract(varCubePreInput);
        }

        BF_newDumpDot(*this,(winningPositions & initSys),NULL,"/tmp/resultR.dot");
        BF_newDumpDot(*this,initEnv.Implies((winningPositions & initSys)).ExistAbstract(varCubePreOutput),NULL,"/tmp/resultA.dot");

        // Get rid of the PreMotionState
        result = result.ExistAbstract(varCubePreMotionState);

        // Check if the result is well-defind. Might fail after an incorrect modification of the above algorithm
        if (!result.isConstant()) {
            BF_newDumpDot(*this,result,NULL,"/tmp/isRealizable.dot");
            throw "Internal error: Could not establish realizability/unrealizability of the specification!";
        }

        // Return the result in Boolean form.
        realizable = result.isTrue();

    }

    void addAutomaticallyGeneratedLivenessAssumption() {

        // Create a new liveness assumption that says that always eventually, if an action/pre-state
        // combination may lead to a different position, then it is taken
        BF prePostMotionStatesDifferent = mgr.constantFalse();
        for (unsigned int i=0;i<preMotionStateVars.size();i++) {
            prePostMotionStatesDifferent |= (preMotionStateVars[i] ^ postMotionStateVars[i]);
        }
        BF preMotionInputCombinationsThatCanChangeState = (prePostMotionStatesDifferent & robotBDD).ExistAbstract(varCubePostMotionState);
        BF newLivenessAssumption = (!preMotionInputCombinationsThatCanChangeState) | prePostMotionStatesDifferent;
        livenessAssumptions.push_back(newLivenessAssumption);
        if (!(newLivenessAssumption.isTrue())) {
            std::cerr << "Note: Added a liveness assumption that always eventually, we are moving if an action is taken that allows moving.\n";
        }
        BF_newDumpDot(*this,newLivenessAssumption,"PreMotionState PostMotionControlOutput PostMotionState","/tmp/changeMotionStateLivenessAssumption.dot");

        // Make sure that there is at least one liveness assumption and one liveness guarantee
        // The synthesis algorithm might be unsound otherwise
        if (livenessGuarantees.size()==0) livenessGuarantees.push_back(mgr.constantTrue());
        if (livenessAssumptions.size()==0) livenessAssumptions.push_back(mgr.constantTrue());

        BF_newDumpDot(*this,robotBDD,"PreMotionState PostMotionControlOutput PostMotionState","/tmp/sometestbdd.dot");
    }


    /**
     * @brief This function orchestrates the execution of slugs when this plugin is used.
     */
    void execute() {
        checkRealizability();
        if (realizable) {
            std::cerr << "RESULT: Specification is realizable.\n";
        } else {
            std::cerr << "RESULT: Specification is unrealizable.\n";
        }
    }


    void bddInspector(std::map<std::string,BF> bdds) {
        std::cerr << "BDD Inspector\n";

        // Main loop
        while (true) {
            std::cerr << "> ";
            std::string command;
            std::getline(std::cin,command);
            std::istringstream commandReader(command);
            std::string operation;
            commandReader >> operation;
            if (!(commandReader.fail())) {
                std::transform(operation.begin(), operation.end(), operation.begin(), toupper);
                if (operation=="QUIT") {
                    return;
                } else if (operation=="LIST") {
                    std::cerr << "BDDs defined:\n";
                    for (auto it : bdds) {
                        std::cerr << "- " << it.first << "\t" << it.second.getSize() << std::endl;
                    }
                } else if (operation=="COPY") {
                    std::string from;
                    commandReader >> from;
                    std::string to;
                    commandReader >> to;
                    if (commandReader.fail()) {
                        std::cerr << "Error: Two parameters expected.\n";
                    } else {
                        if (bdds.count(from)==0) {
                            std::cerr << "Error: BDD '" << from << "' not found.\n";
                        } else {
                            std::cerr << "Copy from: " << from << " to " << to << std::endl;
                            bdds[to] = bdds[from];
                        }
                    }
                } else if (operation=="RESTRICT") {
                    std::string from;
                    commandReader >> from;
                    if (commandReader.fail()) {
                        std::cerr << "Error: A parameters expected.\n";
                    } else if (bdds.count(from)==0) {
                        std::cerr << "Error: BDD '" << from << "' not found.\n";
                    } else {
                        BF result = bdds[from];
                        for (unsigned int i=0;i<variables.size();i++) {
                            std::cerr << "- " << variableNames[i] << ": ";
                            BF test = (result & variables[i]).ExistAbstractSingleVar(variables[i]);
                            if (test==result) {
                                std::cerr << " not in (suffix-)support\n";
                            } else if ((result & variables[i]).isFalse()) {
                                std::cerr << " 0 (forced)\n";
                            } else if ((result & !(variables[i])).isFalse()) {
                                std::cerr << " 1 (forced)\n";
                            } else {
                                std::string line;
                                std::getline(std::cin,line);
                                if (line=="1") {
                                    result &= variables[i];
                                } else if (line=="0") {
                                    result &= !(variables[i]);
                                } else if ((line=="X") || (line=="x")) {
                                    // Nothing.
                                } else {
                                    std::cerr << "     (sorry, didn't get it -- assuming 'X')\n";
                                    //result &= !(variables[i]);
                                }
                            }
                        }
                        std::cerr << "Final result: " << result.getSize() << " nodes.";
                        bdds[from] = result;
                    }
                } else if (operation=="DOT") {
                    std::string from;
                    commandReader >> from;
                    std::string filename;
                    commandReader >> filename;
                    if (commandReader.fail()) {
                        std::cerr << "Error: Two parameters expected.\n";
                    } else if (bdds.count(from)==0) {
                        std::cerr << "Error: BDD '" << from << "' not found.\n";
                    } else {
                        BF_newDumpDot(*this,bdds[from],"Pre Post",filename);
                    }

                } else {
                    std::cerr << "Error: Unrecognized command: " << operation << std::endl;
                }
            }
        }
    }

    /**
     * @brief Prints a state to Stderr
     * @param preState
     */
    void printPreState(BF preState) {

        if (preState.isFalse()) {
            std::cerr << "False\n";
            return;
        }

        std::map<std::string,int> values;
        for (unsigned int i=0;i<variables.size();i++) {
            if (doesVariableInheritType(i,Pre)) {

                // True or not?
                int value = 0;
                if ((preState & variables[i]).isFalse()) {
                    value = 0;
                } else if ((preState & !(variables[i])).isFalse()) {
                    value = 1;
                } else {
                    std::cerr << "Indetermined.\n";
                    return;
                }

                auto atPos = variableNames[i].find("@");
                if (atPos==std::string::npos) {
                    values[variableNames[i]] = value;
                } else {

                    std::string baseName = variableNames[i].substr(0,atPos);
                    std::string rest = variableNames[i].substr(atPos+1,std::string::npos);

                    // Find a dot now
                    auto firstDotPos = rest.find(".");
                    if (firstDotPos==std::string::npos) {
                        std::istringstream rd(rest);
                        int bit;
                        rd >> bit;
                        if (rd.fail()) throw "RDFAIL";
                        values[baseName] += value*(1 << bit);
                    } else {
                        // This is part one
                        if (rest.substr(0,firstDotPos)!="0") throw "IllegalEncoding.";
                        rest = rest.substr(firstDotPos+1,std::string::npos);
                        auto nextDot = rest.find(".");
                        std::string minPart = rest.substr(0,nextDot);
                        std::istringstream minReader(minPart);
                        int min;
                        minReader >> min;
                        if (minReader.fail()) throw "MINFAIL.";
                        values[baseName] += value + min;
                    }
                }
            }

        }

        // Print
        for (auto it : values) {
            std::cerr << it.first << ": " << it.second << ", ";
        }
        std::cerr << std::endl;


    }



};

#endif
