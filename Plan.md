## SISO detection 的调整

- [x] 定义最朴素的siso（一种特殊的siso），它只包含si和so两点，以及si连向so的一条边（这一条边的入边唯一，只是SI）。对于这种siso，额外要求si没有其他outgoing edges，以及so没有incoming edges，否则不视为siso（也就是说对于只包括两个点的siso，有额外要求：si/so本身的输出边数和输入变数也受限制了）.可以对这种siso做特殊标记，方便rewrite识别。在log中，输出siso数目时，输出该类siso的数量。


## Graph rewriter需要实现

## 需要evaluation主义的：

- [ ] bdd build里的计时，增加输出variable reordering产生的时间（用cudd的库可以获得这部分信息）