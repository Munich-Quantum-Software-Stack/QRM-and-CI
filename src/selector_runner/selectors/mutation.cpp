/* Mutation routines */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "nsga2.hpp"
#include "rand.hpp"

/* Function to perform mutation in a population */
void mutation_pop(NSGA2Type *nsga2Params, population *pop)
{
    int i;
    for (i = 0; i < nsga2Params->popsize; i++)
    {
        mutation_ind(nsga2Params, &(pop->ind[i]));
    }
    return;
}

/* Function to perform mutation of an individual */
void mutation_ind(NSGA2Type *nsga2Params, individual *ind)
{
    if (nsga2Params->nreal != 0)
    {
        real_mutate_ind(nsga2Params, ind);
    }
    if (nsga2Params->nint != 0)
    {
        int_mutate_ind(nsga2Params, ind);
        // swapMutation(nsga2Params, ind);
        // insertionMutation(nsga2Params, ind);
        // scrambleMutation(nsga2Params, ind);
        // inversionMutation(nsga2Params, ind);
        // displacementMutation(nsga2Params, ind);
        // cycleMutation(nsga2Params, ind);
    }
    if (nsga2Params->nbin != 0)
    {
        bin_mutate_ind(nsga2Params, ind);
    }
    return;
}

/* Routine for binary mutation of an individual */
void bin_mutate_ind(NSGA2Type *nsga2Params, individual *ind)
{
    int j, k;
    double prob;
    for (j = 0; j < nsga2Params->nbin; j++)
    {
        for (k = 0; k < nsga2Params->nbits[j]; k++)
        {
            prob = randomperc();
            if (prob <= nsga2Params->pmut_bin[j])
            {
                if (ind->gene[j][k] == 0)
                {
                    ind->gene[j][k] = 1;
                }
                else
                {
                    ind->gene[j][k] = 0;
                }
                nsga2Params->nbinmut += 1;
            }
        }
    }
    return;
}

/* Routine for real polynomial mutation of an individual */
void real_mutate_ind(NSGA2Type *nsga2Params, individual *ind)
{
    int j;
    double rnd, delta1, delta2, mut_pow, deltaq;
    double y, yl, yu, val, xy;
    for (j = 0; j < nsga2Params->nreal; j++)
    {
        if (randomperc() <= nsga2Params->pmut_real)
        {
            y = ind->xreal[j];
            yl = nsga2Params->min_realvar[j];
            yu = nsga2Params->max_realvar[j];
            delta1 = (y - yl) / (yu - yl);
            delta2 = (yu - y) / (yu - yl);
            rnd = randomperc();
            mut_pow = 1.0 / (nsga2Params->eta_m + 1.0);
            if (rnd <= 0.5)
            {
                xy = 1.0 - delta1;
                val = 2.0 * rnd +
                      (1.0 - 2.0 * rnd) * (pow(xy, (nsga2Params->eta_m + 1.0)));
                deltaq = pow(val, mut_pow) - 1.0;
            }
            else
            {
                xy = 1.0 - delta2;
                val = 2.0 * (1.0 - rnd) +
                      2.0 * (rnd - 0.5) * (pow(xy, (nsga2Params->eta_m + 1.0)));
                deltaq = 1.0 - (pow(val, mut_pow));
            }
            y = y + deltaq * (yu - yl);
            if (y < yl)
                y = yl;
            if (y > yu)
                y = yu;
            ind->xreal[j] = y;
            nsga2Params->nrealmut += 1;
        }
    }
    return;
}

/* Routine for integer polynomial mutation of an individual */
void int_mutate_ind(NSGA2Type *nsga2Params, individual *ind)
{
    int j;
    int tmp;
    int idx;

    for (j = 0; j < nsga2Params->nint; j++)
    {
        if (randomperc() <= nsga2Params->pmut_int)
        {
            ind->xint[j] =
                rnd(nsga2Params->min_intvar[j], nsga2Params->max_intvar[j]);
            nsga2Params->nintmut += 1;
        }
    }
    return;
}

void swapMutation(NSGA2Type *nsga2Params, individual *ind)
{
    int idx1, idx2;
    int tmp;

    for (int i = 0; i < nsga2Params->nint; i++)
    {
        if (randomperc() <= nsga2Params->pmut_int)
        {
            // Randomly select two positions to swap
            idx1 = rnd(0, nsga2Params->nint - 1);
            idx2 = rnd(0, nsga2Params->nint - 1);

            // Swap the values at the selected positions
            tmp = ind->xint[idx1];
            ind->xint[idx1] = ind->xint[idx2];
            ind->xint[idx2] = tmp;

            nsga2Params->nintmut += 1;
        }
    }
}

void insertionMutation(NSGA2Type *nsga2Params, individual *ind)
{
    int idx1, idx2;
    int tmp;

    for (int i = 0; i < nsga2Params->nint; i++)
    {
        if (randomperc() <= nsga2Params->pmut_int)
        {
            // Randomly select two positions
            idx1 = rnd(0, nsga2Params->nint - 1);
            idx2 = rnd(0, nsga2Params->nint - 1);

            // Remove the element at idx1 and insert it at idx2
            tmp = ind->xint[idx1];
            for (int j = idx1; j < idx2; j++)
            {
                ind->xint[j] = ind->xint[j + 1];
            }
            ind->xint[idx2] = tmp;

            nsga2Params->nintmut += 1;
        }
    }
}

void scrambleMutation(NSGA2Type *nsga2Params, individual *ind)
{
    int idx1, idx2;
    int tmp;

    for (int i = 0; i < nsga2Params->nint; i++)
    {
        if (randomperc() <= nsga2Params->pmut_int)
        {
            // Randomly select two positions
            idx1 = rnd(0, nsga2Params->nint - 1);
            idx2 = rnd(0, nsga2Params->nint - 1);

            // Scramble the elements between idx1 and idx2
            if (idx1 > idx2)
                std::swap(idx1, idx2);
            while (idx1 < idx2)
            {
                tmp = ind->xint[idx1];
                ind->xint[idx1] = ind->xint[idx2];
                ind->xint[idx2] = tmp;
                idx1++;
                idx2--;
            }

            nsga2Params->nintmut += 1;
        }
    }
}

void inversionMutation(NSGA2Type *nsga2Params, individual *ind)
{
    int idx1, idx2;
    int tmp;

    for (int i = 0; i < nsga2Params->nint; i++)
    {
        if (randomperc() <= nsga2Params->pmut_int)
        {
            // Randomly select two positions
            idx1 = rnd(0, nsga2Params->nint - 1);
            idx2 = rnd(0, nsga2Params->nint - 1);

            // Invert the elements between idx1 and idx2
            if (idx1 > idx2)
                std::swap(idx1, idx2);
            while (idx1 < idx2)
            {
                tmp = ind->xint[idx1];
                ind->xint[idx1] = ind->xint[idx2];
                ind->xint[idx2] = tmp;
                idx1++;
                idx2--;
            }

            nsga2Params->nintmut += 1;
        }
    }
}

void displacementMutation(NSGA2Type *nsga2Params, individual *ind)
{
    int idx1, idx2, segmentSize;
    int tmp;

    for (int i = 0; i < nsga2Params->nint; i++)
    {
        if (randomperc() <= nsga2Params->pmut_int)
        {
            // Randomly select two positions
            idx1 = rnd(0, nsga2Params->nint - 1);
            idx2 = rnd(0, nsga2Params->nint - 1);

            // Ensure idx1 < idx2
            if (idx1 > idx2)
                std::swap(idx1, idx2);

            // Determine segment size
            segmentSize = idx2 - idx1 + 1;

            // Create a temporary array to store the segment
            int segment[segmentSize];

            // Copy the segment to the temporary array
            for (int j = 0; j < segmentSize; j++)
            {
                segment[j] = ind->xint[idx1 + j];
            }

            // Remove the segment from the permutation
            for (int j = idx1; j < nsga2Params->nint - segmentSize; j++)
            {
                ind->xint[j] = ind->xint[j + segmentSize];
            }

            // Update permutation size
            nsga2Params->nint -= segmentSize;

            // Randomly select a position to insert the segment
            int insertPos = rnd(0, nsga2Params->nint - 1);

            // Shift elements to make space for the segment
            for (int j = nsga2Params->nint - 1; j >= insertPos; j--)
            {
                ind->xint[j + segmentSize] = ind->xint[j];
            }

            // Insert the segment at the selected position
            for (int j = 0; j < segmentSize; j++)
            {
                ind->xint[insertPos + j] = segment[j];
            }

            // Update permutation size
            nsga2Params->nint += segmentSize;

            nsga2Params->nintmut += 1;
        }
    }
}

void cycleMutation(NSGA2Type *nsga2Params, individual *ind)
{
    int idx1, idx2;
    int tmp;

    for (int i = 0; i < nsga2Params->nint; i++)
    {
        if (randomperc() <= nsga2Params->pmut_int)
        {
            // Randomly select two positions
            idx1 = rnd(0, nsga2Params->nint - 1);
            idx2 = rnd(0, nsga2Params->nint - 1);

            // Perform a cyclic permutation within the cycle defined by idx1 and
            // idx2
            std::vector<int> cycle;
            if (idx1 != idx2)
            {
                int start = idx1;
                do
                {
                    cycle.push_back(ind->xint[start]);
                    start = ind->xint[start];
                } while (start != idx1);
            }

            // Apply cyclic permutation to the cycle
            std::rotate(cycle.begin(), cycle.begin() + 1, cycle.end());

            // Update the permutation with the new cycle
            int cycleIdx = 0;
            int cycleSize = cycle.size();
            for (int j = idx1; j != idx2; j = ind->xint[j])
            {
                ind->xint[j] = cycle[cycleIdx];
                cycleIdx = (cycleIdx + 1) % cycleSize;
            }

            nsga2Params->nintmut += 1;
        }
    }
}
