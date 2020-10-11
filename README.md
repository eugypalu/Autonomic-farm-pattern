# Autonomic farm pattern

This repository contains a university project, the goal is to provide a farm pattern ensuring (best effort) a given service time leveraging on dynamic variation of the parallelism degree. The farm is instantiated and run by providing:

* A collection of input tasks to be computed (of type Tin)
* A function<Tout(Tin)> computing the single task
* An expected service time TSgoal
* An initial parallelism degree nw

During farm execution, autonomic farm management should increase or decrease the parallelism degree in such a way its service time is as close as possible to the expected service time TSgoal .

The pattern should be tested providing a collection of tasks such that the tasks in the initial, central and final part all require a different average time to be computed (e.g. 4L in the first part, L in the second part and 8L in the third part) and the task collection execution time is considerably longer than the time needed to reconfigure the farm.
