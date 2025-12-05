## SISO detection 的调整

- [x] 定义最朴素的siso（一种特殊的siso），它只包含si和so两点，以及si连向so的一条边（这一条边的入边唯一，只是SI）。对于这种siso，额外要求si没有其他outgoing edges，以及so没有incoming edges，否则不视为siso（也就是说对于只包括两个点的siso，有额外要求：si/so本身的输出边数和输入变数也受限制了）.可以对这种siso做特殊标记，方便rewrite识别。在log中，输出siso数目时，输出该类siso的数量。
- [ ] detection 控制目标siso的大小/类别
- [ ] detection 标记其他特殊（trivial/etc）siso，方便rewriter fast path
- [ ] 单conj的siso+input facts的组合（现在对input facts的处理是好的吗）

目前analyzer的基础设施，感觉是用来寻找复杂siso的，但是至少暂时在side channel的例子里，非常简单的siso就足够有用了，它们几乎都只有一条或两条边（我们可以先关注只有一条边的）。你是否可以把旧的analyzer的逻辑archive，但是不动暴露给rewriter的接口

## Graph rewriter 的优化

- [ ] fast path. 对于fast path siso，
- [x] dd manager的初始化，只在需要的时候发生，应当视为某种thunk.

思考：对于现在的side channel，是否因为conj密集，没有disj，导致分解效果弱？

## 需要evaluation注意的：

- [ ] bdd build里的计时，增加输出variable reordering产生的时间（用cudd的库可以获得这部分信息）
- [ ] debug 激活方式统一