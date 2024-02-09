#ifndef MY_QDMI_HPP
#define MY_QDMI_HPP

struct Job
{
    int job_id;
    int task_id;
    float execution_time;

    Job(int job_id, int task_id, float execution_time)
        : job_id(job_id), task_id(task_id), execution_time(execution_time)
    {
    }
};

#endif // MY_QDMI_HPP