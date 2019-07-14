读本目录下的代码，可能会用到的一些知识点:
### 汇编指令
- ltr指令 load task register,loads the current task register with the value specified in "src". 即把src的值赋值给任务寄存器
- lldt指令 load local descriptor table, loads a value from an operand into local descriptor table register, 即从给定的位置处加载相应数据到ldtr中。
- str指令 store task register, 即store the current task register to the specified operand, 即把任务寄存器的值存放到给定的位置上。
- clts指令 clear task switched flag in mathcine status register.
- ror指令 rotate right, rotate the bits in the destination to the right "count"times with all data pushed out the right side re-entering on the left. 即进行向右循环移位操作。
- lsl指令 load segment limit, load the segment limit of a selector into the destination register if the selector is valid and visible at the current privilege level.If loaing is successful, teh Zero flag is set, otherwise it is cleared.
- 
