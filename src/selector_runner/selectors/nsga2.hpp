/* This file contains the variable and function declarations */

#ifndef NSGA2_HPP
#define NSGA2_HPP

#include <assert.h>
#include <iostream>
#include <math.h>
#include <memory>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <vector>

#include <llvm/IR/InstrTypes.h>
#include <llvm/IR/Module.h>

using namespace llvm;

#define INF 1.0e14
#define EPS 1.0e-14
#define eul 2.71828182845905
#define pi 3.14159265358979
#define GNUPLOT_COMMAND "gnuplot -persist"

typedef struct
{
    int rank;
    double constr_violation;
    double *xreal;
    int *xint;
    int **gene;
    double *xbin;
    double *obj;
    double *constr;
    double crowd_dist;
} individual;

typedef struct
{
    individual *ind;
} population;

typedef struct lists
{
    int index;
    struct lists *parent;
    struct lists *child;
} list;

typedef struct NSGA2Type
{
    double seed;
    int nreal;
    int nint;
    int nbin;
    int nsim;
    int nobj;
    int ncon;
    int popsize;
    double pcross_real;
    double pcross_int;
    double pcross_bin;
    double pmut_real;
    double pmut_int;
    double *pmut_bin;
    double eta_c;
    double eta_m;
    int ngen;
    int nbinmut;
    int nintmut;
    int nrealmut;
    int nbincross;
    int nintcross;
    int nrealcross;
    int *nbits;
    double *min_realvar;
    double *max_realvar;
    int *min_intvar;
    int *max_intvar;
    double *min_binvar;
    double *max_binvar;
    int bitlength;
    int choice;
    int obj1;
    int obj2;
    int obj3;
    int angle1;
    int angle2;
} NSGA2Type;

// Global
extern FILE *fpt1;
extern FILE *fpt2;
extern FILE *fpt3;
extern FILE *fpt4;
extern FILE *fpt5;
extern FILE *gp;
extern population *parent_pop;
extern population *child_pop;
extern population *mixed_pop;

/**
 *  allocate.c
 */
void allocate_memory_pop(NSGA2Type *nsga2Params, population *pop, int size);
void allocate_memory_ind(NSGA2Type *nsga2Params, individual *ind);
void deallocate_memory_pop(NSGA2Type *nsga2Params, population *pop, int size);
void deallocate_memory_ind(NSGA2Type *nsga2Params, individual *ind);

double maximum(double a, double b);
double minimum(double a, double b);

void crossover(NSGA2Type *nsga2Params, individual *parent1, individual *parent2,
               individual *child1, individual *child2);
void realcross(NSGA2Type *nsga2Params, individual *parent1, individual *parent2,
               individual *child1, individual *child2);
void intcross(NSGA2Type *nsga2Params, individual *parent1, individual *parent2,
              individual *child1, individual *child2);
void bincross(NSGA2Type *nsga2Params, individual *parent1, individual *parent2,
              individual *child1, individual *child2);

void assign_crowding_distance_list(NSGA2Type *nsga2Params, population *pop,
                                   list *lst, int front_size);
void assign_crowding_distance_indices(NSGA2Type *nsga2Params, population *pop,
                                      int c1, int c2);
void assign_crowding_distance(NSGA2Type *nsga2Params, population *pop,
                              int *dist, int **obj_array, int front_size);

void decode_pop(NSGA2Type *nsga2Params, population *pop);
void decode_ind(NSGA2Type *nsga2Params, individual *ind);

void onthefly_display(NSGA2Type *nsga2Params, population *pop, FILE *gp, int ii,
                      int xtop, int ytop, int ztop);

int check_dominance(NSGA2Type *nsga2Params, individual *a, individual *b);

void evaluate_pop(NSGA2Type *nsga2Params, population *pop,
                  std::unique_ptr<Module> &module,
                  const std::vector<std::string> designSpace, bool fVerbose);
void evaluate_ind(NSGA2Type *nsga2Params, individual *ind,
                  std::unique_ptr<Module> &module,
                  const std::vector<std::string> designSpace, bool fVerbose);

void fill_nondominated_sort(NSGA2Type *nsga2Params, population *mixed_pop,
                            population *new_pop);
void crowding_fill(NSGA2Type *nsga2Params, population *mixed_pop,
                   population *new_pop, int count, int front_size, list *cur);

void initialize_pop(NSGA2Type *nsga2Params, population *pop);
void initialize_ind(NSGA2Type *nsga2Params, individual *ind);

void insert(list *node, int x);
list *del(list *node);

void merge(NSGA2Type *nsga2Params, population *pop1, population *pop2,
           population *pop3);
void copy_ind(NSGA2Type *nsga2Params, individual *ind1, individual *ind2);

void mutation_pop(NSGA2Type *nsga2Params, population *pop);
void mutation_ind(NSGA2Type *nsga2Params, individual *ind);
void bin_mutate_ind(NSGA2Type *nsga2Params, individual *ind);
void int_mutate_ind(NSGA2Type *nsga2Params, individual *ind);
void real_mutate_ind(NSGA2Type *nsga2Params, individual *ind);

void test_problem(double *xreal, double *xbin, int **gene, double *obj,
                  double *constr);
void test_qir(NSGA2Type *nsga2Params, int **gene, double *obj,
              std::unique_ptr<Module> &module, int fVerbose);
void assign_rank_and_crowding_distance(NSGA2Type *nsga2Params,
                                       population *new_pop);

void ranges_pop(NSGA2Type *nsga2Params, population *pop, int *xtop, int *ytop,
                int *ztop);
void report_pop(NSGA2Type *nsga2Params, population *pop, FILE *fpt);
void report_feasible(NSGA2Type *nsga2Params, population *pop, FILE *fpt);
void report_ind(individual *ind, FILE *fpt);

void quicksort_front_obj(population *pop, int objcount, int obj_array[],
                         int obj_array_size);
void q_sort_front_obj(population *pop, int objcount, int obj_array[], int left,
                      int right);
void quicksort_dist(population *pop, int *dist, int front_size);
void q_sort_dist(population *pop, int *dist, int left, int right);

void selection(NSGA2Type *nsga2Params, population *old_pop,
               population *new_pop);
individual *tournament(NSGA2Type *nsga2Params, individual *ind1,
                       individual *ind2);

/**
 * nsga2.c
 */
NSGA2Type ReadParameters(int sizeChrom, int nobj);
int InitNSGA2(NSGA2Type *nsga2Params, std::unique_ptr<Module> &module,
              const char *local_path, bool fVerbose,
              const std::vector<std::string> designSpace);
int NSGA2(NSGA2Type *nsga2Params, std::unique_ptr<Module> &module,
          bool fVerbose, const std::vector<std::string> designSpace);
void print_nsga2Params(NSGA2Type *nsga2Params);

#endif // NSGA2_HPP
