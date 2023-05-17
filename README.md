# Concurrent Elevator System
An M story high-rise has *N* elevators in a single “elevator bank”, each able to serve every floor of the building. Contrary from most elevator designs, there are no buttons to choose a destination floor inside the elevator. Instead, the elevator lobby at each floor has M individual “destination floor” buttons, and *P* passengers choose their destination by pushing the appropriate button. Once a button is pushed, a display near the buttons tells the passenger which elevator door to wait by. In this assignment, elevators serve only one passenger at a time.

Concurrency info:
* There is a total of N+P threads.
* Each [elevator](https://github.com/sharbel97/concurrent-elevators/blob/main/controller.c#L19) has a mutex and a conditional variable.
* Each [passenger](https://github.com/sharbel97/concurrent-elevators/blob/main/controller.c#L7) has a conditional variable.


#### 1 Elevator, 6 floors, 10 passengers 1 trip each.
https://github.com/sharbel97/concurrent-elevators/assets/38640642/9bafe542-4ad5-4790-84fd-63525a89ee86




#### 4 Elevators, 12 floors, 20 passengers 1 trip each.
https://github.com/sharbel97/concurrent-elevators/assets/38640642/5f9561f7-51fd-4369-b044-695ed4938e2b
