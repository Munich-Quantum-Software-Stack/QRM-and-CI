/**
 * @file nsga2.cpp
 * @brief NSGA-II routine (implementation of the 'main' function)
 */

#include "nsga2.hpp"
#include "rand.hpp"

using llvm::orc::ThreadSafeModule;

FILE *fpt1;
FILE *fpt2;
FILE *fpt3;
FILE *fpt4;
FILE *fpt5;
FILE *fpt6;
FILE *gp;

population *parent_pop;
population *child_pop;
population *mixed_pop;

static int xtop = 0;
static int ytop = 0;
static int ztop = 0;

NSGA2Type ReadParameters(int sizeChrom, int nobj, QDMI_Device &device)
{
    NSGA2Type nsga2Params;
    int i;

    srand(time(NULL));

    nsga2Params.seed = (float)rand() / (float)(RAND_MAX); // Seed value
    nsga2Params.popsize = 64;      // Population size (multiple of 4)
    nsga2Params.ngen = 64;        // Number of generations
    nsga2Params.nobj = nobj;      // Number of objectives
    nsga2Params.ncon = 0;         // Number of constraints
    nsga2Params.nreal = 0;        // Number of real variables
    nsga2Params.nint = sizeChrom; // Number of integer variables
    nsga2Params.nbin = 0;         // Number of binary variables
    nsga2Params.nsim = 0;         // Number of simulations
    nsga2Params.device = device;

    assert(nsga2Params.seed > 0.0 && nsga2Params.seed < 1.0);
    assert(nsga2Params.popsize >= 4 && (nsga2Params.popsize % 4) == 0);

    assert(nsga2Params.ngen > 0);
    assert(nsga2Params.nobj > 0);
    assert(nsga2Params.ncon >= 0);
    assert(nsga2Params.nint >= 0);
    // assert(nsga2Params.nbin > 0);

    std::cout << "                         L ...seed value: "
              << nsga2Params.seed << std::endl;
    std::cout << "                         L ...population size: "
              << nsga2Params.popsize << std::endl;
    std::cout << "                         L ...number of generations: "
              << nsga2Params.ngen << std::endl;
    std::cout << "                         L ...number of objectives: "
              << nsga2Params.nobj << std::endl;
    std::cout << "                         L ...number of integer variables: "
              << nsga2Params.nint << std::endl;

    // nsga2Params.nbits = (int *)malloc(nsga2Params.nbin * sizeof(int));
    nsga2Params.min_intvar = (int *)malloc(nsga2Params.nint * sizeof(int));
    nsga2Params.max_intvar = (int *)malloc(nsga2Params.nint * sizeof(int));
    // nsga2Params.pmut_real = (double)malloc(sizeof(double));
    nsga2Params.pcross_int =
        0.6; // Probability of crossover of int variable (0.6-1.0)

    for (i = 0; i < nsga2Params.nint; i++)
    {
        nsga2Params.min_intvar[i] = 0;
        nsga2Params.max_intvar[i] = sizeChrom - 1;

        // nsga2Params.nbits[i] = sizeChrom;
        nsga2Params.pmut_int = 0.5; //(double)1/nsga2Params.nbits[i];

        assert(nsga2Params.pmut_int >= 0.0 && nsga2Params.pmut_int <= 1.0);
    }

    assert(nsga2Params.pcross_int >= 0.0 && nsga2Params.pcross_int <= 1.0);

    std::cout << "                         L ...crossover probability: "
              << nsga2Params.pcross_int << std::endl;

    nsga2Params.choice =
        0; // Use gnuplot to display the results realtime (0 for NO) (1 for yes)

    // assert(nsga2Params.nreal ^ nsga2Params.nbin);
    std::cout << "[DEBUG] end of nsga2" << std::endl;
    return nsga2Params;
}

int InitNSGA2(NSGA2Type *nsga2Params, ThreadSafeModule &TSM,
              const char *local_path, bool fVerbose,
              const std::vector<std::string> designSpace)
{
    int i;

    // Initialize the files...
    char local_intial_pop[strlen(local_path) + 100];
    char local_final_pop[strlen(local_path) + 100];
    char local_best_pop[strlen(local_path) + 100];
    char local_all_pop[strlen(local_path) + 100];
    char local_params[strlen(local_path) + 100];
    char local_feasible_pop[strlen(local_path) + 100];

    std::cout << "[DEBUG] Before snprintf" << std::endl;

    snprintf(local_intial_pop, sizeof(local_intial_pop), "%s%s", local_path,
             "initial_pop.out");
    snprintf(local_final_pop, sizeof(local_final_pop), "%s%s", local_path,
             "final_pop.out");
    snprintf(local_best_pop, sizeof(local_best_pop), "%s%s", local_path,
             "best_pop.out");
    snprintf(local_all_pop, sizeof(local_all_pop), "%s%s", local_path,
             "all_pop.out");
    snprintf(local_params, sizeof(local_params), "%s%s", local_path,
             "params.out");
    snprintf(local_feasible_pop, sizeof(local_feasible_pop), "%s%s", local_path,
             "feasible_pop.out");

    fpt1 = fopen(local_intial_pop, "w");
    fpt2 = fopen(local_final_pop, "w");
    fpt3 = fopen(local_best_pop, "w");
    fpt4 = fopen(local_all_pop, "w");
    fpt5 = fopen(local_params, "w");
    fpt6 = fopen(local_feasible_pop, "w");

    assert(nsga2Params->nint > 0);
    std::cout << "[DEBUG] After assert" << std::endl;

    fprintf(fpt1, "# This file contains the data of initial population\n");
    fprintf(fpt2, "# This file contains the data of final population\n");
    fprintf(fpt3, "# This file contains the data of final feasible population "
                  "(if found)\n");
    fprintf(fpt4, "# This file contains the data of all generations\n");
    fprintf(fpt5, "# This file contains information about inputs as read by "
                  "the program\n");

    fprintf(fpt5, "\n Population size                              = %d",
            nsga2Params->popsize);
    fprintf(fpt5, "\n Number of generations                       = %d",
            nsga2Params->ngen);
    fprintf(fpt5, "\n Number of objective functions               = %d",
            nsga2Params->nobj);
    fprintf(fpt5, "\n Number of constraints                       = %d",
            nsga2Params->ncon);
    fprintf(fpt5, "\n Number of integer variables                 = %d",
            nsga2Params->nint);
    fprintf(fpt5, "\n Probability of crossover of int variable    = %e",
            nsga2Params->pcross_int);

    // nsga2Params->bitlength = 0;
    std::cout << "[DEBUG] Before loop" << std::endl;
    for (i = 0; i < nsga2Params->nint; i++)
    {
        // fprintf(fpt5, "\n Number of bits for binary variable %d         =
        // %d",
        //        i + 1, nsga2Params->nbits[i]);
        fprintf(fpt5, "\n Probability of mutation of integer variable %d = %e",
                i + 1, nsga2Params->pmut_int);

        // nsga2Params->bitlength += nsga2Params->nbits[i];
    }

    fprintf(fpt5, "\n Seed for random number generator = %e",
            nsga2Params->seed);

    fprintf(
        fpt1,
        "# of objectives = %d, # of constraints = %d, # of int_var = %d, # of "
        "bits of bin_var = %d, constr_violation, rank, crowding_distance\n",
        nsga2Params->nobj, nsga2Params->ncon, nsga2Params->nint,
        nsga2Params->nbin);
    fprintf(
        fpt2,
        "# of objectives = %d, # of constraints = %d, # of int_var = %d, # of "
        "bits of bin_var = %d, constr_violation, rank, crowding_distance\n",
        nsga2Params->nobj, nsga2Params->ncon, nsga2Params->nint,
        nsga2Params->nbin);
    fprintf(
        fpt3,
        "# of objectives = %d, # of constraints = %d, # of int_var = %d, # of "
        "bits of bin_var = %d, constr_violation, rank, crowding_distance\n",
        nsga2Params->nobj, nsga2Params->ncon, nsga2Params->nint,
        nsga2Params->nbin);
    fprintf(
        fpt4,
        "# of objectives = %d, # of constraints = %d, # of int_var = %d, # of "
        "bits of bin_var = %d, constr_violation, rank, crowding_distance\n",
        nsga2Params->nobj, nsga2Params->ncon, nsga2Params->nint,
        nsga2Params->nbin);

    nsga2Params->nbinmut = 0;
    nsga2Params->nintmut = 0;
    nsga2Params->nrealmut = 0;
    nsga2Params->nbincross = 0;
    nsga2Params->nintcross = 0;
    nsga2Params->nrealcross = 0;

    std::cout << "[DEBUG] Before population initialization" << std::endl;
    // Initializing the populations
    parent_pop = (population *)malloc(sizeof(population));
    child_pop = (population *)malloc(sizeof(population));
    mixed_pop = (population *)malloc(sizeof(population));

    allocate_memory_pop(nsga2Params, parent_pop, nsga2Params->popsize);
    allocate_memory_pop(nsga2Params, child_pop, nsga2Params->popsize);
    allocate_memory_pop(nsga2Params, mixed_pop, 2 * nsga2Params->popsize);

    // Preparing first Population
    randomize(nsga2Params->seed);
    initialize_pop(nsga2Params, parent_pop);

    evaluate_pop(nsga2Params, parent_pop, TSM, designSpace, fVerbose);
    assign_rank_and_crowding_distance(nsga2Params, parent_pop);
    report_pop(nsga2Params, parent_pop, fpt1);
    report_feasible(nsga2Params, parent_pop, fpt6);

    std::cout << "[DEBUG] after population things" << std::endl;
    char buff[100];
    time_t now = time(0);
    strftime(buff, 100, "%Y-%m-%d %H:%M:%S.000", localtime(&now));

    fprintf(fpt4, "\n# gen = 1, time = %s\n", buff);

    if (fVerbose)
        printf(" # gen = 1, time = %s\n", buff);

    report_pop(nsga2Params, parent_pop, fpt4);

    std::cout << "[DEBUG] after report pop" << std::endl;
    /*
    if (nsga2Params->choice != 0)
    {
        ranges_pop(nsga2Params, parent_pop, &xtop, &ytop, &ztop);
        onthefly_display(nsga2Params, parent_pop, gp, 1, xtop, ytop, ztop);
    }
    */

    fflush(fpt1);
    fflush(fpt2);
    fflush(fpt3);
    fflush(fpt4);
    fflush(fpt5);
    fflush(fpt6);

    return 0;
}

std::vector<std::string> NSGA2(NSGA2Type *nsga2Params, ThreadSafeModule &TSM, bool fVerbose,
          const std::vector<std::string> designSpace)
{
    int i;
    char buff[100];

    for (i = 2; i <= nsga2Params->ngen; i++)
    {
        selection(nsga2Params, parent_pop, child_pop);
        std::cout << "DEBUG A\n";
        mutation_pop(nsga2Params, child_pop);
        //    	decode_pop(nsga2Params, child_pop);
        std::cout << "DEBUG B\n";
        evaluate_pop(nsga2Params, child_pop, TSM, designSpace, fVerbose);
        std::cout << "DEBUG C\n";
        merge(nsga2Params, parent_pop, child_pop, mixed_pop);
        std::cout << "DEBUG D\n";
        fill_nondominated_sort(nsga2Params, mixed_pop, parent_pop);

        time_t now = time(0);
        strftime(buff, 100, "%Y-%m-%d %H:%M:%S.000", localtime(&now));

        fprintf(fpt4, "\n# gen = %d, time = %s\n", i, buff);

        if (fVerbose)
            printf(" # gen = %d, time = %s\n", i, buff);

        report_pop(nsga2Params, parent_pop, fpt4);
        std::cout << "DEBUG E\n";
        fflush(fpt4);

        report_feasible(nsga2Params, parent_pop, fpt6);
    }

    // printf("\n\n Generations finished, now reporting solutions");

    report_pop(nsga2Params, parent_pop, fpt2);
    report_feasible(nsga2Params, parent_pop, fpt3);

    if (nsga2Params->nint != 0)
    {
        fprintf(fpt5, "\n Number of crossover of int variable = %d",
                nsga2Params->nintcross);
        fprintf(fpt5, "\n Number of mutation of int variable  = %d",
                nsga2Params->nintmut);
    }

    individual best_individual = parent_pop->ind[0];

    for (i = 1; i < nsga2Params->popsize; i++) {
        if (check_dominance(nsga2Params, &(parent_pop->ind[i]),
            &(best_individual)) == 1)
        {
            best_individual = parent_pop->ind[i];
        }
    } 

    std::vector<std::string> result;

    for (i = 0; i < nsga2Params->nint; i++)
    {
        result.push_back(designSpace[best_individual.xint[i]]);
    }

    // Closing the files and freeing up memories...
    fflush(fpt1);
    fflush(fpt2);
    fflush(fpt3);
    fflush(fpt4);
    fflush(fpt5);
    fflush(fpt6);

    fclose(fpt1);
    fclose(fpt2);
    fclose(fpt3);
    fclose(fpt4);
    fclose(fpt5);
    fclose(fpt6);

    if (nsga2Params->choice != 0)
        pclose(gp);

    if (nsga2Params->nint != 0)
    {
        free(nsga2Params->min_intvar);
        free(nsga2Params->max_intvar);
        // free(nsga2Params->nbits);
    }

    deallocate_memory_pop(nsga2Params, parent_pop, nsga2Params->popsize);
    deallocate_memory_pop(nsga2Params, child_pop, nsga2Params->popsize);
    deallocate_memory_pop(nsga2Params, mixed_pop, 2 * nsga2Params->popsize);

    free(parent_pop);
    free(child_pop);
    free(mixed_pop);

    printf("\n Routine successfully finished \n");

    return result;
}
