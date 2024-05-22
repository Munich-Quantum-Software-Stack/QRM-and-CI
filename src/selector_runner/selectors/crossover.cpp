/* Crossover routines */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "nsga2.hpp"
#include "rand.hpp"

/* Function to cross two individuals */
void crossover(NSGA2Type *nsga2Params, individual *parent1, individual *parent2,
               individual *child1, individual *child2)
{
    if (nsga2Params->nreal != 0)
    {
        realcross(nsga2Params, parent1, parent2, child1, child2);
    }
    if (nsga2Params->nint != 0)
    {
        // intcross(nsga2Params, parent1, parent2, child1, child2);
        // intInterleafCross(nsga2Params, parent1, parent2, child1, child2);
        intTwoPointCross(nsga2Params, parent1, parent2, child1, child2);
        // partiallyMappedCrossover(nsga2Params, parent1, parent2, child1,
        // child2); cycleCrossover(nsga2Params, parent1, parent2, child1,
        // child2);
    }
    if (nsga2Params->nbin != 0)
    {
        bincross(nsga2Params, parent1, parent2, child1, child2);
    }
    return;
}

/* Routine for real variable SBX crossover */
void realcross(NSGA2Type *nsga2Params, individual *parent1, individual *parent2,
               individual *child1, individual *child2)
{
    int i;
    double rand;
    double y1, y2, yl, yu;
    double c1, c2;
    double alpha, beta, betaq;
    if (randomperc() <= nsga2Params->pcross_real)
    {
        nsga2Params->nrealcross++;
        for (i = 0; i < nsga2Params->nreal; i++)
        {
            if (randomperc() <= 0.5)
            {
                if (fabs(parent1->xreal[i] - parent2->xreal[i]) > EPS)
                {
                    if (parent1->xreal[i] < parent2->xreal[i])
                    {
                        y1 = parent1->xreal[i];
                        y2 = parent2->xreal[i];
                    }
                    else
                    {
                        y1 = parent2->xreal[i];
                        y2 = parent1->xreal[i];
                    }
                    yl = nsga2Params->min_realvar[i];
                    yu = nsga2Params->max_realvar[i];
                    rand = randomperc();
                    beta = 1.0 + (2.0 * (y1 - yl) / (y2 - y1));
                    alpha = 2.0 - pow(beta, -(nsga2Params->eta_c + 1.0));
                    if (rand <= (1.0 / alpha))
                    {
                        betaq = pow((rand * alpha),
                                    (1.0 / (nsga2Params->eta_c + 1.0)));
                    }
                    else
                    {
                        betaq = pow((1.0 / (2.0 - rand * alpha)),
                                    (1.0 / (nsga2Params->eta_c + 1.0)));
                    }
                    c1 = 0.5 * ((y1 + y2) - betaq * (y2 - y1));
                    beta = 1.0 + (2.0 * (yu - y2) / (y2 - y1));
                    alpha = 2.0 - pow(beta, -(nsga2Params->eta_c + 1.0));
                    if (rand <= (1.0 / alpha))
                    {
                        betaq = pow((rand * alpha),
                                    (1.0 / (nsga2Params->eta_c + 1.0)));
                    }
                    else
                    {
                        betaq = pow((1.0 / (2.0 - rand * alpha)),
                                    (1.0 / (nsga2Params->eta_c + 1.0)));
                    }
                    c2 = 0.5 * ((y1 + y2) + betaq * (y2 - y1));
                    if (c1 < yl)
                        c1 = yl;
                    if (c2 < yl)
                        c2 = yl;
                    if (c1 > yu)
                        c1 = yu;
                    if (c2 > yu)
                        c2 = yu;
                    if (randomperc() <= 0.5)
                    {
                        child1->xreal[i] = c2;
                        child2->xreal[i] = c1;
                    }
                    else
                    {
                        child1->xreal[i] = c1;
                        child2->xreal[i] = c2;
                    }
                }
                else
                {
                    child1->xreal[i] = parent1->xreal[i];
                    child2->xreal[i] = parent2->xreal[i];
                }
            }
            else
            {
                child1->xreal[i] = parent1->xreal[i];
                child2->xreal[i] = parent2->xreal[i];
            }
        }
    }
    else
    {
        for (i = 0; i < nsga2Params->nreal; i++)
        {
            child1->xreal[i] = parent1->xreal[i];
            child2->xreal[i] = parent2->xreal[i];
        }
    }
    return;
}

/* Routine for int variable SBX crossover */
void intcross(NSGA2Type *nsga2Params, individual *parent1, individual *parent2,
              individual *child1, individual *child2)
{
    int i;
    double rand;
    double y1, y2, yl, yu;
    double c1, c2;
    double alpha, beta, betaq;
    if (randomperc() <= nsga2Params->pcross_int)
    {
        nsga2Params->nintcross++;
        for (i = 0; i < nsga2Params->nint; i++)
        {
            if (randomperc() <= 0.5)
            {
                // if (fabs(parent1->xint[i] - parent2->xint[i]) > EPS)
                if (parent1->xint[i] != parent2->xint[i])
                {
                    if (parent1->xint[i] < parent2->xint[i])
                    {
                        y1 = parent1->xint[i];
                        y2 = parent2->xint[i];
                    }
                    else
                    {
                        y1 = parent2->xint[i];
                        y2 = parent1->xint[i];
                    }
                    yl = nsga2Params->min_intvar[i];
                    yu = nsga2Params->max_intvar[i];
                    rand = randomperc();
                    beta = 1.0 + (2.0 * (y1 - yl) / (y2 - y1));
                    alpha = 2.0 - pow(beta, -(nsga2Params->eta_c + 1.0));
                    if (rand <= (1.0 / alpha))
                    {
                        betaq = pow((rand * alpha),
                                    (1.0 / (nsga2Params->eta_c + 1.0)));
                    }
                    else
                    {
                        betaq = pow((1.0 / (2.0 - rand * alpha)),
                                    (1.0 / (nsga2Params->eta_c + 1.0)));
                    }
                    c1 = 0.5 * ((y1 + y2) - betaq * (y2 - y1));
                    beta = 1.0 + (2.0 * (yu - y2) / (y2 - y1));
                    alpha = 2.0 - pow(beta, -(nsga2Params->eta_c + 1.0));
                    if (rand <= (1.0 / alpha))
                    {
                        betaq = pow((rand * alpha),
                                    (1.0 / (nsga2Params->eta_c + 1.0)));
                    }
                    else
                    {
                        betaq = pow((1.0 / (2.0 - rand * alpha)),
                                    (1.0 / (nsga2Params->eta_c + 1.0)));
                    }
                    c2 = 0.5 * ((y1 + y2) + betaq * (y2 - y1));
                    if (c1 < yl)
                        c1 = yl;
                    if (c2 < yl)
                        c2 = yl;
                    if (c1 > yu)
                        c1 = yu;
                    if (c2 > yu)
                        c2 = yu;

                    if (c1 > 35)
                    {
                        c1 = 35;
                    }
                    if (c2 > 35)
                    {
                        c2 = 35;
                    }

                    if (randomperc() <= 0.5)
                    {
                        child1->xint[i] = (int)round(c2);
                        child2->xint[i] = (int)round(c1);
                    }
                    else
                    {
                        child1->xint[i] = (int)round(c1);
                        child2->xint[i] = (int)round(c2);
                    }
                }
                else
                {
                    child1->xint[i] = parent1->xint[i];
                    child2->xint[i] = parent2->xint[i];
                }
            }
            else
            {
                child1->xint[i] = parent1->xint[i];
                child2->xint[i] = parent2->xint[i];
            }
        }
    }
    else
    {
        for (i = 0; i < nsga2Params->nint; i++)
        {
            child1->xint[i] = parent1->xint[i];
            child2->xint[i] = parent2->xint[i];
        }
    }
    return;
}

/* Routine for int variable interleaf crossover */
void intInterleafCross(NSGA2Type *nsga2Params, individual *parent1,
                       individual *parent2, individual *child1,
                       individual *child2)
{
    int i;
    if (randomperc() <= nsga2Params->pcross_int)
    {
        nsga2Params->nintcross++;
        for (i = 0; i < nsga2Params->nint; i++)
        {
            if (i % 2 == 0)
            {
                child1->xint[i] = parent1->xint[i];
                child2->xint[i] = parent2->xint[i];
            }
            else
            {
                child1->xint[i] = parent2->xint[i];
                child2->xint[i] = parent1->xint[i];
            }
        }
    }
    else
    {
        for (i = 0; i < nsga2Params->nint; i++)
        {
            child1->xint[i] = parent1->xint[i];
            child2->xint[i] = parent2->xint[i];
        }
    }
    return;
}

/* Routine for two point integer crossover */
void intTwoPointCross(NSGA2Type *nsga2Params, individual *parent1,
                      individual *parent2, individual *child1,
                      individual *child2)
{
    int i, j;
    double rand;
    int temp, site1, site2;
    // for (i = 0; i < nsga2Params->nint; i++)
    //{
    rand = randomperc();
    if (rand <= nsga2Params->pcross_int)
    {
        nsga2Params->nintcross++;
        site1 = rnd(0, nsga2Params->nint - 1);
        site2 = rnd(0, nsga2Params->nint - 1);
        if (site1 > site2)
        {
            temp = site1;
            site1 = site2;
            site2 = temp;
        }

        for (i = 0; i < site1; i++)
        {
            child1->xint[i] = parent1->xint[i];
            child2->xint[i] = parent2->xint[i];
        }
        for (i = site1; i < site2; i++)
        {
            child1->xint[i] = parent2->xint[i];
            child2->xint[i] = parent1->xint[i];
        }
        for (i = site2; i < nsga2Params->nint; i++)
        {
            child1->xint[i] = parent1->xint[i];
            child2->xint[i] = parent2->xint[i];
        }
        } else
        {
            for (i = 0; i < nsga2Params->nint; i++)
            {
                child1->xint[i] = parent1->xint[i];
                child2->xint[i] = parent2->xint[i];
            }
        }
        return;
    }

    void cycleCrossover(NSGA2Type * nsga2Params, individual * parent1,
                        individual * parent2, individual * child1,
                        individual * child2)
    {
        if (randomperc() <= nsga2Params->pcross_int)
        {
            nsga2Params->nintcross++;
            bool cycle = true;
            std::vector<bool> visited(nsga2Params->nint, false);
            int numVisited = 0;

            for (int i = 0; i < nsga2Params->nint; i++)
            {
                if (!visited[i])
                {
                    int startIdx = i;
                    do
                    {
                        if (cycle)
                        {
                            child1->xint[i] = parent1->xint[i];
                            child2->xint[i] = parent2->xint[i];
                        }
                        else
                        {
                            child1->xint[i] = parent2->xint[i];
                            child2->xint[i] = parent1->xint[i];
                        }
                        visited[i] = true;
                        numVisited++;
                        for (int j = 0; j < nsga2Params->nint; j++)
                        {
                            if (parent1->xint[j] == parent1->xint[i])
                            {
                                i = j;
                                break;
                            }
                        }
                    } while (i != startIdx && !visited[i]);

                    cycle = !cycle;
                }
            }

            // Terminate the loop if all elements are visited
            if (numVisited == nsga2Params->nint)
                return;
        }
        else
        {
            for (int i = 0; i < nsga2Params->nint; i++)
            {
                child1->xint[i] = parent1->xint[i];
                child2->xint[i] = parent2->xint[i];
            }
        }
    }

    void partiallyMappedCrossover(NSGA2Type * nsga2Params, individual * parent1,
                                  individual * parent2, individual * child1,
                                  individual * child2)
    {
        double rand = randomperc();
        int nint = nsga2Params->nint;
        if (rand <= nsga2Params->pcross_int)
        {
            nsga2Params->nintcross++;
            int site1 = rnd(0, nint - 1);
            int site2 = rnd(0, nint - 1);
            if (site1 > site2)
                std::swap(site1, site2);

            // Copy genes from parent1 to child1 and from parent2 to child2
            // within the crossover sites
            for (int j = site1; j <= site2; j++)
            {
                child1->xint[j] = parent1->xint[j];
                child2->xint[j] = parent2->xint[j];
            }

            // Fill the remaining genes in child1 and child2 using parent2 and
            // parent1, respectively
            for (int j = 0; j < nint; j++)
            {
                if (j < site1 || j > site2)
                {
                    int gene1 = parent1->xint[j];
                    int gene2 = parent2->xint[j];
                    // Find the corresponding gene from parent2 in child1
                    auto it =
                        std::find(child1->xint, child1->xint + nint, gene2);
                    if (it != child1->xint + nint)
                    {
                        gene2 = parent2->xint[it - child1->xint];
                    }
                    // Find the corresponding gene from parent1 in child2
                    it = std::find(child2->xint, child2->xint + nint, gene1);
                    if (it != child2->xint + nint)
                    {
                        gene1 = parent1->xint[it - child2->xint];
                    }
                    child1->xint[j] = gene2;
                    child2->xint[j] = gene1;
                }
            }
        }
        else
        {
            // If crossover does not occur, copy genes from parents to children
            // directly
            for (int j = 0; j < nint; j++)
            {
                child1->xint[j] = parent1->xint[j];
                child2->xint[j] = parent2->xint[j];
            }
        }
    }

    /* Routine for two point binary crossover */
    void bincross(NSGA2Type * nsga2Params, individual * parent1,
                  individual * parent2, individual * child1,
                  individual * child2)
    {
        int i, j;
        double rand;
        int temp, site1, site2;
        for (i = 0; i < nsga2Params->nbin; i++)
        {
            rand = randomperc();
            if (rand <= nsga2Params->pcross_bin)
            {
                nsga2Params->nbincross++;
                site1 = rnd(0, nsga2Params->nbits[i] - 1);
                site2 = rnd(0, nsga2Params->nbits[i] - 1);
                if (site1 > site2)
                {
                    temp = site1;
                    site1 = site2;
                    site2 = temp;
                }
                for (j = 0; j < site1; j++)
                {
                    child1->gene[i][j] = parent1->gene[i][j];
                    child2->gene[i][j] = parent2->gene[i][j];
                }
                for (j = site1; j < site2; j++)
                {
                    child1->gene[i][j] = parent2->gene[i][j];
                    child2->gene[i][j] = parent1->gene[i][j];
                }
                for (j = site2; j < nsga2Params->nbits[i]; j++)
                {
                    child1->gene[i][j] = parent1->gene[i][j];
                    child2->gene[i][j] = parent2->gene[i][j];
                }
            }
            else
            {
                for (j = 0; j < nsga2Params->nbits[i]; j++)
                {
                    child1->gene[i][j] = parent1->gene[i][j];
                    child2->gene[i][j] = parent2->gene[i][j];
                }
            }
        }
        return;
    }
