#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>

#define SQRT_2 sqrt(2)

typedef struct {
	char board[7][6][2];
	char blockers[4];
	char player_turn;
} GameState;

struct TreeNode {
	struct TreeNode** parents;
	struct TreeNode* children[21];
	char* move_after_parent; //Length cooresponds with num_parents
	unsigned char num_parents;
	char turn_from_root;
	char expanded;
	char turn_number;
	unsigned int node_score;
	unsigned int node_visits;
	unsigned int node_draws;
	char printed;
	char counted;
};

struct ThreadInfo {
	struct TreeNode* root;
	char cont_exec;
	int seconds;
};

struct LinkedNode {
	int from_file;
	struct TreeNode* new_pointer;
	struct LinkedNode* left;
	struct LinkedNode* right;
};

char randint() { //This is solely to make the random numbers uniform
	int end = (RAND_MAX / 21) * 21;
	int ret;
	while((ret = rand()) >= end);

	return (char) (ret % 21);
}

char actionOrDecision() {
	int input;
	do {
		printf("Make AI do a decision or make an enemy move (0=AI/1=Enemy): ");
		scanf("%d", &input);
	} while(input != 0 && input != 1);
	return (char)input;
}

//Assumptions:
//node->node_visits will be at least 1
double UCB1(struct TreeNode* node) {
	//parent->node_visits will always be at least 1
	double avg_numerator = (double) (node->node_score + node->node_draws);
	double avg_denomenator = (double) (node->node_visits << 1);
	double avg_score = avg_numerator / avg_denomenator;

	double sqrt_numerator = 0;
	for(int i = 0; i < node->num_parents; i++) { //Exploration: because node has multiple parents, add their visits
		sqrt_numerator += node->parents[i]->node_visits;
	}
	sqrt_numerator = log(sqrt_numerator);
	double sqrt_denomenator = node->node_visits;
	double sqrt_score = sqrt(sqrt_numerator / sqrt_denomenator);
	return avg_score + SQRT_2*sqrt_score;
}

//START FUNCTIONS FOR GAME
char checkHorizontal(GameState gameState, char i, char j, char player) {
	if(i > 3) return 0;
	return ((gameState.board[i][j][0] == player || gameState.board[i][j][1] == player) &&
			(gameState.board[i+1][j][0] == player || gameState.board[i+1][j][1] == player) &&
			(gameState.board[i+2][j][0] == player || gameState.board[i+2][j][1] == player) &&
			(gameState.board[i+3][j][0] == player || gameState.board[i+3][j][1] == player));
}

char checkVertical(GameState gameState, char i, char j, char player) {
	if(j > 2) return 0;
	return ((gameState.board[i][j][0] == player || gameState.board[i][j][1] == player) &&
			(gameState.board[i][j+1][0] == player || gameState.board[i][j+1][1] == player) &&
			(gameState.board[i][j+2][0] == player || gameState.board[i][j+2][1] == player) &&
			(gameState.board[i][j+3][0] == player || gameState.board[i][j+3][1] == player));
}

char checkDiagonal_DL_UR(GameState gameState, char i, char j, char player) {
	if(i > 3 || j > 2) return 0;
	return ((gameState.board[i][j][0] == player || gameState.board[i][j][1] == player) &&
			(gameState.board[i+1][j+1][0] == player || gameState.board[i+1][j+1][1] == player) &&
			(gameState.board[i+2][j+2][0] == player || gameState.board[i+2][j+2][1] == player) &&
			(gameState.board[i+3][j+3][0] == player || gameState.board[i+3][j+3][1] == player));
}

char checkDiagonal_UL_DR(GameState gameState, char i, char j, char player) {
	if(i > 3 || j < 3) return 0;
	return ((gameState.board[i][j][0] == player || gameState.board[i][j][1] == player) &&
			(gameState.board[i+1][j-1][0] == player || gameState.board[i+1][j-1][1] == player) &&
			(gameState.board[i+2][j-2][0] == player || gameState.board[i+2][j-2][1] == player) &&
			(gameState.board[i+3][j-3][0] == player || gameState.board[i+3][j-3][1] == player));
}

char terminalState(GameState gameState) {
	for(int i = 0; i < 7; i++) {
		for(int j = 0; j < 6; j++) {
			for(char player = 1; player <= 4; player++) {
				if(checkHorizontal(gameState, i, j, player) ||
				   checkVertical(gameState, i, j, player) ||
				   checkDiagonal_DL_UR(gameState, i, j, player) ||
				   checkDiagonal_UL_DR(gameState, i, j, player)) return player;
			}
		}
	}
	//Not over yet or draw
	char draw = 1;
	for(int i = 0; i < 7; i++) {
		if(gameState.board[i][5][0] == 0 || gameState.board[i][5][1] == 0) {
			draw = 0;
			break;
		}
	}
	if(draw) return -1; //-1==Draw
	else return 0; //0==Game not finished yet
}

GameState initializeGame() {
	GameState istate;
	for(int i = 0; i < 7; i++) {
		for(int j = 0; j < 6; j++) {
			istate.board[i][j][0] = 0;
			istate.board[i][j][1] = 0;
		}
	}
	for(int i = 0; i < 4; i++) {
		istate.blockers[i] = 2;
	}
	istate.player_turn = 1;
	return istate;
}

void printState(GameState gameState) {
	printf("________________________\n");
	printf("  |C0|C1|C2|C3|C4|C5|C6|\n");
	for(int j = 5; j >= 0; j--) {
		printf("R%d", j);
		for(int i = 0; i < 7; i++) {
			printf("|%d%d", gameState.board[i][j][0], gameState.board[i][j][1]);
		}
		printf("|\n");
	}
	printf("________________________\n");
	char tState = terminalState(gameState);
	if(!tState) { //If game is still going
		printf("Player %d has %d blockers\n", gameState.player_turn, gameState.blockers[gameState.player_turn-1]);
	} else {
		if(tState > 0) { //If there is a winner
			printf("Player %d has won!\n", (int)tState);
		} else { //If no one won
			printf("It's a draw!\n");
		}
	}
}

char canDoAction(GameState gameState, char action) {
	if(action >= 21 || action < 0) return 0;
	int col = action % 7;
	int side = action / 7;
	if(side == 2) {
		if(gameState.blockers[gameState.player_turn-1] <= 0) return 0;
		return gameState.board[col][5][0] == 0 && gameState.board[col][5][1] == 0;
	}
	return gameState.board[col][5][side] == 0;
}

char getAction(GameState gameState) {
	char action = -1;
	do {
		printf("Pick a column to drop a piece in: C");
		int col;
		scanf("%d", &col);

		printf("What side do you want to drop it in (0=Left/1=Right/2=Both): ");
		int side;
		scanf("%d", &side);

		action = 7*side + col;
	} while(!canDoAction(gameState, action));
	return action;
}

char isColumnLeveled(GameState gameState, char col) {
	char leveled = 1;
	for(int i = 5; i >= 0; i--) {
		if(gameState.board[col][i][0] == 0 && gameState.board[col][i][1] == 0) {
			continue;
		}
		if(gameState.board[col][i][0] == 0 || gameState.board[col][i][1] == 0) {
			leveled = 0;
		}
		break;
	}
	return leveled;
}

char* availableActions(GameState gameState) {
	if(terminalState(gameState)) return NULL;
	char moves = 0;
	for(int i = 0; i < 7; i++) {
		if(canDoAction(gameState, i)) moves++;
		if(!isColumnLeveled(gameState, i) && canDoAction(gameState, i+7)) moves++; //For AI, lower branching factor by only using right side if column isn't leveled
		if(canDoAction(gameState, i+14)) moves++;
	}
	char* actions = malloc(sizeof(char)*moves+1);
	int ptr = 0;
	for(int i = 0; i < 7; i++) {
		if(canDoAction(gameState, i)) actions[ptr++] = i;
		if(!isColumnLeveled(gameState, i) && canDoAction(gameState, i+7)) actions[ptr++] = i+7;
		if(canDoAction(gameState, i+14)) actions[ptr++] = i+14;
	}
	actions[moves] = -1;
	return actions;
}

GameState doAction(GameState gameState, char action) {
	char col = action % 7;
	char side = action / 7;
	char row = 5;
	if(side == 2) {
		for(int j = 4; j >= 0; j--) {
			if(gameState.board[col][j][0] == 0 && gameState.board[col][j][1] == 0) {
				row = j;
			} else break;
		}
		gameState.board[col][row][0] = gameState.player_turn;
		gameState.board[col][row][1] = gameState.player_turn;
		gameState.blockers[gameState.player_turn-1]--;
	} else {
		for(int j = 4; j >= 0; j--) {
			if(gameState.board[col][j][side] == 0) {
				row = j;
			} else break;
		}
		gameState.board[col][row][side] = gameState.player_turn;
	}
	gameState.player_turn = (gameState.player_turn % 4) + 1;
	return gameState;
}
//END FUNCTIONS FOR GAME

//START GameState AI Tree save/restore features
void printNodeToFile(FILE* fp, struct TreeNode* node) {
	if(node->printed) return;
	fprintf(fp, "%p = {\n\tnum_parents = %u\n\tparents = {", node, (unsigned int) node->num_parents);
	for(int i = 0; i < node->num_parents; i++) {
		fprintf(fp, " %p", node->parents[i]);
	}
	fprintf(fp, " }\n\tchildren = {");
	for(int i = 0; i < 21; i++) {
		if(node->children[i]) fprintf(fp, " %p", node->children[i]);
		else fprintf(fp, " 0x0");
	}
	fprintf(fp, " }\n\tmove_after_parent = {");
	for(int i = 0; i < node->num_parents; i++) {
		fprintf(fp, " %d", (int)node->move_after_parent[i]);
	}
	fprintf(fp, " }\n\tturn_from_root = %d\n\texpanded = %d\n\tturn_number = %d", (int) node->turn_from_root, (int) node->expanded, (int) node->turn_number);
	fprintf(fp, "\n\tnode_score = %u\n\tnode_visits = %u\n\tnode_draws = %u\n}\n\n", node->node_score, node->node_visits, node->node_draws);
	node->printed = 1;
	for(int i = 0; i < 21; i++) {
		if(node->children[i] == NULL) continue;
		printNodeToFile(fp, node->children[i]);
	}
}

void resetPrintValues(struct TreeNode* root) {
	root->printed = 0;
	for(int i = 0; i < 21; i++) if(root->children[i]) resetPrintValues(root->children[i]);
}

void printTreeToFile(FILE* fp, struct TreeNode* root, char action) {
	while(root->parents != NULL) {
		root = root->parents[0];
	}
	fprintf(fp, "%d\n", (int) action);
	printNodeToFile(fp, root);
	resetPrintValues(root);
}

void destroyLinkedTree(struct LinkedNode* node) {
	if(node->left) destroyLinkedTree(node->left);
	if(node->right) destroyLinkedTree(node->right);
	free(node);
}

struct TreeNode* getPointer(struct LinkedNode** start, int file_ptr) {
	struct LinkedNode* prev = NULL;
	struct LinkedNode* moving = *start;
	while(moving != NULL && moving->from_file != file_ptr) {
		prev = moving;
		if(file_ptr < moving->from_file) moving = moving->left;
		else moving = moving->right;
	}
	if(moving == NULL) {
		moving = malloc(sizeof(struct LinkedNode));
		if(*start == NULL) *start = moving;
		if(prev != NULL) {
			if(file_ptr < prev->from_file) prev->left = moving;
			else prev->right = moving;
		}
		moving->from_file = file_ptr;
		moving->new_pointer = malloc(sizeof(struct TreeNode));
	}
	return moving->new_pointer;
}

struct TreeNode* readTreeFromFile(FILE* fp, char* action) {
	int a;
	fscanf(fp, "%d\n", &a);
	*action = (char) a;
	struct LinkedNode* start = NULL;
	while(!feof(fp)) {
		int file_ptr;
		unsigned int np;
		fscanf(fp, "0x%x = {\n\tnum_parents = %u\n\tparents = {", &file_ptr, &np);

		struct TreeNode* node = getPointer(&start, file_ptr);
		node->num_parents = (unsigned char) np;

		if(np) { //Initialize parents && moves from them if we're not root
			node->parents = malloc(sizeof(struct TreeNode*) * np);
			node->move_after_parent = malloc(sizeof(char) * np);
		}
		else {
			node->parents = NULL;
			node->move_after_parent = NULL;
		}
		for(int i = 0; i < np; i++) { //Get Parents
			int p_ptr;
			fscanf(fp, " 0x%x", &p_ptr);
			node->parents[i] = getPointer(&start, p_ptr);
		}
		fscanf(fp, " }\n\tchildren = {");
		for(int i = 0; i < 21; i++) { //Get Children
			int c_ptr;
			fscanf(fp, " 0x%x", &c_ptr);
			if(c_ptr) node->children[i] = getPointer(&start, c_ptr);
			else node->children[i] = NULL;
		}
		fscanf(fp, " }\n\tmove_after_parent = {");
		for(int i = 0; i < np; i++) { //Get Moves from Parents
			int m;
			fscanf(fp, " %d", &m);
			node->move_after_parent[i] = (char) m;
		}

		int t_root, ex, t_num;
		fscanf(fp, " }\n\tturn_from_root = %d\n\texpanded = %d\n\tturn_number = %d", &t_root, &ex, &t_num);
		node->turn_from_root = (char) t_root;
		node->expanded = (char) ex;
		node->turn_number = (char) t_num;

		unsigned int n_s, n_v, n_d;
		fscanf(fp, "\n\tnode_score = %u\n\tnode_visits = %u\n\tnode_draws = %u\n}\n\n", &n_s, &n_v, &n_d);
		node->node_score = n_s;
		node->node_visits = n_v;
		node->node_draws = n_d;
		node->printed = 0;
		node->counted = 0;
	}
	struct TreeNode* root = start->new_pointer;
	destroyLinkedTree(start);
	while(1) {
		for(int i = 0; i < 21; i++) {
			if(root->children[i]) {
				if(root->children[i]->turn_from_root) return root;
				root = root->children[i];
				break;
			}
		}
	}
}
//END GameState AI Tree save/restore features

//START FUNCTIONS FOR AI
char getMove(struct TreeNode* root) {	
	char max_index = -1;
	double max_win_rate = -1.0;
	for(int i = 0; i < 21; i++) {
		struct TreeNode* child = root->children[i];
		if(child == NULL) continue;
		double node_win_rate = ((double)(child->node_score + child->node_draws)) / ((double)(child->node_visits << 1));
		if(max_win_rate < node_win_rate) {
			max_index = i;
			max_win_rate = node_win_rate;
		}
	}
	return max_index;
}

void flipChildren(struct TreeNode* node, char action) { //Assumes action >= 7 && action < 14
        struct TreeNode* leftChild = node->children[action - 7];
        struct TreeNode* rightChild = node->children[action];

        if(leftChild) {
                for(int i = 0; i < leftChild->num_parents; i++) {
                        if(leftChild->parents[i] == node) {
                                leftChild->move_after_parent[i] = action;
                                break;
                        }
                }
        }
        if(rightChild) {
                for(int i = 0; i < rightChild->num_parents; i++) {
                        if(rightChild->parents[i] == node) {
                                rightChild->move_after_parent[i] = action - 7;
                                break;
                        }
                }
        }

        node->children[action - 7] = rightChild;
        node->children[action] = leftChild;
        //Don't take blockers into account, they're level ground, thus not needed for flipping
        for(int i = 0; i < 14; i++) {
                if(node->children[i] == NULL) continue;
                flipChildren(node->children[i], action);
        }
}

void fixNodeValues(struct TreeNode* node, unsigned int node_score, unsigned int node_visits, unsigned int node_draws, char player) {
	//Backpropagate through tree with values from duplicate child
	if((node->turn_number % 4) + 1 == player) {
		node->node_score += node_score;
	}
	node->node_visits += node_visits;
	node->node_draws += node_draws;
	if(node->parents != NULL) {
		for(int p = 0; p < node->num_parents; p++) {
			fixNodeValues(node->parents[p], node_score, node_visits, node_draws, player);
		}
	}
}

void destroyBranch(struct TreeNode* destroy, struct TreeNode* parent) { //Destroy every child of destroy
	if(destroy->num_parents == 1) { //If node to be destroyed only has one parent, destroy each child
		for(char i = 0; i < 21; i++) {
			if(destroy->children[i] == NULL) continue;
			destroyBranch(destroy->children[i], destroy);
			destroy->children[i] = NULL;
		}
		free(destroy->parents);
		destroy->parents = (struct TreeNode**) 0x11111111;
		free(destroy->move_after_parent);
		destroy->move_after_parent = (char*) 0x11111111;
		free(destroy);
	} else { //If node has > 1 parents, then unlink the destoy node from the parent node
		int parent_index = -1;
		for(int i = 0; i < destroy->num_parents; i++) {
			if(destroy->parents[i] == parent) {
				parent_index = i;
				break;
			}
		}
		struct TreeNode** new_parents = malloc(sizeof(struct TreeNode*) * destroy->num_parents-1);
		char* new_moves = malloc(sizeof(char) * destroy->num_parents-1);
		int ptr = 0;
		for(int i = 0; i < destroy->num_parents; i++) {
			if(i == parent_index) continue;
			new_parents[ptr] = destroy->parents[i];
			new_moves[ptr++] = destroy->move_after_parent[i];
		}
		destroy->num_parents--;
		free(destroy->parents);
		free(destroy->move_after_parent);
		destroy->parents = new_parents;
		destroy->move_after_parent = new_moves;
	}
}

void destroyExcessBranches(struct TreeNode* root, char move) { //Destroy each branch except for the moveth child of root
	for(char i = 0; i < 21; i++) {
		if(root->children[i] == NULL || i == move) continue;
		destroyBranch(root->children[i], root);
		root->children[i] = NULL;
	}
}

void resetValues(struct TreeNode* node) { //Refer to resetTree()
	node->turn_from_root = node->parents[0]->turn_from_root + 1;

	for(int i = 0; i < 21; i++) {
		if(node->children[i] == NULL) continue;
		resetValues(node->children[i]);
	}
}

void resetTree(struct TreeNode* root) { //Reset turn_from_root value for all nodes from root
	root->turn_from_root = 0;

	for(int i = 0; i < 21; i++) {
		if(root->children[i] == NULL) continue;
		resetValues(root->children[i]);
	}
}

void initializeNode(struct TreeNode* node, struct TreeNode* parent, char move) {
	if(parent == NULL) { //If we are the inital state
		node->parents = NULL;
		node->move_after_parent = NULL;
		node->num_parents = 0;
		node->turn_number = 0;
		node->turn_from_root = 0;
	}
	else { //We are not the initial state
		node->parents = malloc(sizeof(struct TreeNode*));
		node->parents[0] = parent;
		node->move_after_parent = malloc(sizeof(char));
		node->move_after_parent[0] = move;
		node->num_parents = 1;
		node->turn_number = parent->turn_number + 1;
		node->turn_from_root = parent->turn_from_root + 1;
	}
	for(int i = 0; i < 21; i++) {
		node->children[i] = NULL;
	}
	node->expanded = 0;
	node->node_score = 0;
	node->node_visits = 0;
	node->node_draws = 0;
	node->printed = 0;
	node->counted = 0;
}

GameState convertNodeToBoard(struct TreeNode* node) { //Do actions from initial game state down to input node
	if(node->parents == NULL) {
		return initializeGame();
	}
	GameState gstate = convertNodeToBoard(node->parents[0]);
	return doAction(gstate, node->move_after_parent[0]);
}

char compareStates(GameState g1, GameState g2) { //Assumption: g1 and g2 are on the same turn number
	for(int i = 0; i < 7; i++) {
		for(int j = 0; j < 6; j++) {
			if(g1.board[i][j][0] != g2.board[i][j][0] ||
			   g1.board[i][j][1] != g2.board[i][j][1]) return 0;
		}
	}
	return 1;
}

//node_created is a double pointer because TreeNode* is pass by value
//node_created may be changed for caller, so we need pass by reference
void getDuplicates(struct TreeNode** node_created, struct TreeNode* node_check, char* moves_to_check, char* moves_upto_node, GameState g_created, GameState g_check) {
	if(node_check == NULL) return;
	char player = (((*node_created)->turn_number - 1) % 4) + 1;
	char check_player = ((node_check->turn_number) % 4) + 1;
	if(node_check->turn_number != (*node_created)->turn_number) { //Must be on same turn to be the same
		if(player == check_player) {
			for(int ptr = 0; moves_to_check[ptr] != -1; ptr++) {
				getDuplicates(node_created, node_check->children[moves_to_check[ptr]], moves_to_check, moves_upto_node, g_created, doAction(g_check, moves_to_check[ptr]));
			}
		} else {
			getDuplicates(node_created, node_check->children[moves_upto_node[node_check->turn_from_root]], moves_to_check, moves_upto_node,
				      g_created, doAction(g_check, moves_upto_node[node_check->turn_from_root]));
		}
		return;
	}	
	for(int p1 = 0; p1 < (*node_created)->num_parents; p1++) { //If they have the same parents, it's already done.
		for(int p2 = 0; p2 < node_check->num_parents; p2++) {
			if((*node_created)->parents[p1] == node_check->parents[p2]) return;
		}
	}
	if(compareStates(g_created, g_check)) { //If two states are actually the same
		if((*node_created)->num_parents > 1) {
			printf("%p->num_parents > 1\n", (void *) (*node_created)); //should never be called (keyword should)
		}
		struct TreeNode** new_parents = malloc(sizeof(struct TreeNode*) * (node_check->num_parents + (*node_created)->num_parents));
		char* new_moves = malloc(sizeof(char) * (node_check->num_parents + (*node_created)->num_parents));
		for(int i = 0; i < node_check->num_parents; i++) {
			new_parents[i] = node_check->parents[i];
			new_moves[i] = node_check->move_after_parent[i];
		}

		for(int i = 0; i < (*node_created)->num_parents; i++) {
			new_parents[i + node_check->num_parents] = (*node_created)->parents[i];
			new_moves[i + node_check->num_parents] = (*node_created)->move_after_parent[i];
		}
		
		free(node_check->parents);
		free(node_check->move_after_parent);
		node_check->parents = new_parents;
		node_check->move_after_parent = new_moves;

		for(int i = 0; i < (*node_created)->num_parents; i++) {
			(*node_created)->parents[i]->children[(*node_created)->move_after_parent[i]] = node_check;
			fixNodeValues((*node_created)->parents[i], node_check->node_score, node_check->node_visits, node_check->node_draws, (node_check->turn_number % 4) + 1);
		}
		
		node_check->num_parents += (*node_created)->num_parents;

		free((*node_created)->parents);
		free((*node_created)->move_after_parent);
		free((*node_created));
		(*node_created) = node_check;
	}
}

void findDuplicate(struct TreeNode* node) {
	if(node->turn_from_root < 5) return; //No duplicates possible until 5th move after root
	char player = ((node->turn_number - 1) % 4) + 1;
	char moves_made = ((node->turn_from_root + 3) / 4);
	char* moves_to_check = malloc(sizeof(char) * moves_made + 1);
	char* moves_upto_node = malloc(sizeof(char) * node->turn_from_root);
	struct TreeNode* root = node;
	char check_i = moves_made;
	char upto_i = node->turn_from_root - 1;
	moves_to_check[check_i--] = -1;
	while(root->turn_from_root != 0) {
		if(((root->turn_number - 1) % 4) + 1 == player) {
			moves_to_check[check_i--] = root->move_after_parent[0];
		}
		moves_upto_node[upto_i--] = root->move_after_parent[0];
		root = root->parents[0];
	}
	getDuplicates(&node, root, moves_to_check, moves_upto_node, convertNodeToBoard(node), convertNodeToBoard(root));
	free(moves_to_check);
	free(moves_upto_node);
}

void expandNode(struct TreeNode* node) {
	node->expanded = 1;
	GameState state = convertNodeToBoard(node);
	for(int i = 0; i < 21; i++) { //If any node is terminal, only expand that one
		if(canDoAction(state, i) && terminalState(doAction(state, i))) {
			node->children[i] = malloc(sizeof(struct TreeNode));
			initializeNode(node->children[i], node, i);
			node->children[i]->expanded = 1;
			return;
		}
	}
	char* moves = availableActions(state);
	if(!moves) return;
	char ptr = 0;
	while(moves[ptr] != -1) {
		node->children[moves[ptr]] = malloc(sizeof(struct TreeNode));
		initializeNode(node->children[moves[ptr]], node, moves[ptr]);
		findDuplicate(node->children[moves[ptr]]);
		ptr++;
	}
	free(moves);
}

struct TreeNode* selectNode(struct TreeNode* node) {
	if(node->expanded == 0) {
		if(node->node_visits == 0) { //This is pretty much just here for root
			return node;
		}
		expandNode(node);
		for(int i = 0; i < 21; i++) {
			if(node->children[i] != NULL) return node->children[i];
		}
		return node;
	}
	char max_index = 0;
	double max_UCB1 = -1;
	for(char i = 0; i < 21; i++) {
		if(node->children[i] == NULL) continue;
		if(node->children[i]->node_visits == 0) {
			return node->children[i];
		}
		double UCB1_score = UCB1(node->children[i]);
		if(max_UCB1 < UCB1_score) {
			max_index = i;
			max_UCB1 = UCB1_score;
		}
	}
	if(max_UCB1 == -1) return node; //If node is terminal
	return selectNode(node->children[max_index]);
}

char simulateNode(struct TreeNode* node) {
	GameState gstate = convertNodeToBoard(node);
	while(!terminalState(gstate)) {
		char move = randint();
		if(canDoAction(gstate, move)) {
			gstate = doAction(gstate, move);
		}
	}
	return terminalState(gstate);
}

void backpropagation(struct TreeNode* node, char result) {
	if(result == ((node->turn_number - 1) % 4) + 1) {
		node->node_score += 2;
	} else if(result == -1) {
		node->node_draws++;
	}
	node->node_visits++;
	for(int p = 0; p < node->num_parents; p++) {
		backpropagation(node->parents[p], result);
	}
}

void* MCTS_process(void* input) {
	struct ThreadInfo* t = (struct ThreadInfo*) input;
	struct TreeNode* root = t->root;
	time_t start = time(NULL);
	while(t->cont_exec || difftime(time(NULL), start) < t->seconds) { //Run for a minimum of 20 seconds (end immedietely if enemy makes a move)
		struct TreeNode* selectedNode = selectNode(root);
		char action = simulateNode(selectedNode);
		backpropagation(selectedNode, action);
	}
	pthread_exit(0);
}
//END FUNCTIONS FOR AI

int main(int argc, char** argv) {
	extern char* optarg;
	char iflag = 0, oflag = 0;
	char* ifile;
	int c;
	while((c = getopt(argc, argv, "i:o")) != -1) {
		switch(c) {
			case 'i':
				iflag = 1;
				ifile = optarg;
				break;
			case 'o':
				oflag = 1;
				break;
		}
	}

	if(iflag && oflag) {
		fprintf(stderr, "Usage: [./connect4x4 -i [filename]] OR [./connect4x4 -o] OR  [./connect4x4]");
		exit(1);
	}

	time_t seed = time(NULL);
	srand(seed);
	struct TreeNode* root;

	if(iflag) {
		FILE* fp = fopen(ifile, "r");
		char action;
		root = readTreeFromFile(fp, &action);
		fclose(fp);
		destroyExcessBranches(root, action);
		root = root->children[action];
	}
	else {
		root = malloc(sizeof(struct TreeNode));
		initializeNode(root, NULL, -1);
	}

	GameState gstate = convertNodeToBoard(root);
	int turn = 1;
	while(!terminalState(gstate)) {
		printState(gstate);
		char action;

		struct ThreadInfo* ti = malloc(sizeof(struct ThreadInfo));
		ti->root = root;
		ti->cont_exec = 1;
		ti->seconds = 20;

		pthread_t tid;
		pthread_create(&tid, NULL, MCTS_process, ti);

		if(actionOrDecision()) { //If we want to make an enemy move
			action = getAction(gstate); //Get enemy move, then stop thread
			ti->cont_exec = 0;
			ti->seconds = 0;
			pthread_join(tid, NULL);
		} else { //If we want AI to produce an action for us
			ti->cont_exec = 0; //Stop thread, then get AI move
			pthread_join(tid, NULL);
			action = getMove(root);
		}
		if((action >= 7 && action < 14) && isColumnLeveled(gstate, action - 7)) { 
		    //If enemy moved where AI decided not to look (right side on level ground)
            //Flip everything from that column up until leaf node and don't take blockers into account
            root->children[action] = root->children[action - 7];
            root->children[action]->move_after_parent[0] = action;
            root->children[action - 7] = NULL;
            flipChildren(root->children[action], action);
        }
		gstate = doAction(gstate, action);

		if(oflag) {
			FILE* fp;
			char filename[15];
			sprintf(filename, "output/turn_%d", turn);
			fp = fopen(filename, "w+");
			printTreeToFile(fp, root, action);
			fclose(fp);
			turn++;
		}

		destroyExcessBranches(root, action);
		
		root = root->children[action];
		resetTree(root);
		free(ti);
	}
	printState(gstate);
	return 0;
}