现在，rewrite siso的复杂度，是和子图大小有关，还是和完整图大小有关（也就是某种意义上要操作完整图构造新的完整图，所以即使只改了小图，复杂度也是完整图的复杂度）？哦对了，对于P13，现在facts.prob，no rewrite和rewrite的一致性也还仍然能保证对吧？

## SISO detection 的调整

- [x] 定义最朴素的siso（一种特殊的siso），它只包含si和so两点，以及si连向so的一条边（这一条边的入边唯一，只是SI）。对于这种siso，额外要求si没有其他outgoing edges，以及so没有incoming edges，否则不视为siso（也就是说对于只包括两个点的siso，有额外要求：si/so本身的输出边数和输入变数也受限制了）.可以对这种siso做特殊标记，方便rewrite识别。在log中，输出siso数目时，输出该类siso的数量。
- [ ] detection 控制目标siso的大小/类别
- [ ] detection 标记其他特殊（trivial/etc）siso，方便rewriter fast path
- [ ] 单conj的siso+input facts的组合（现在对input facts的处理是好的吗）

目前analyzer的基础设施，感觉是用来寻找复杂siso的，但是至少暂时在side channel的例子里，非常简单的siso就足够有用了，它们几乎都只有一条或两条边（我们可以先关注只有一条边的）。你是否可以把旧的analyzer的逻辑archive，但是不动暴露给rewriter的接口，同时实现一个只尝试detect有两条边以内的siso，其中只有一条边的siso，包括pure-two-nodes和一般的SingleHyperedge；有两条边的siso，一般是线性结构（a/SI -> b -> c/SO）或者 平行边结构（a -> b \/ a ->' b），也可能有（a1 -> b \/ a2 -> b 同时a1/a2都是input facts这种 all facts as input的结构）。注意到只有pure-two-nodes对SI/SO有额外限制；all facts as SI也有特殊限制，要求每个facts没有其他输入边。我需要你为这些fast path情况，首先增加regionkind，然后在analyzer中实现一个fast detection path，避免旧的重的detection设施的调用。你觉得如何？

整理对所有fast-path siso子图的定义

linear two edge的siso的rewrite，就是将中间的两边一点替换为一条边。强调linear two edge的中间的点需要满足siso的限制（非query/evidence，从detect的阶段就拒绝）。si/so无限制。新边对应的概率，是之前的两条边的概率的积。

## Graph rewriter 的优化

- [ ] fast path. 对于fast path siso，
- [x] dd manager的初始化，只在需要的时候发生，应当视为某种thunk.

思考：对于现在的side channel，是否因为conj密集，没有disj，导致分解效果弱？

在每一轮最后，在simplify一次每一个hyperedge，让它吸收单独的leaf node？

全conj的siso的优化

on-the-fly analyze + rewrite 是否会有收益？

目前性能瓶颈更具体说明

## 需要evaluation注意的：

- [ ] bdd build里的计时，增加输出variable reordering产生的时间（用cudd的库可以获得这部分信息）
- [ ] debug 激活方式统一

跑一下P5/P9/P12，各自rewrite, no rewrite 结果对比，各阶段时间对比，最终forward comp的bdd node count对比

在每一次尝试rewrite所有siso的迭代结束后，尝试收缩一下每一个边：遍历每一条边，找到它的所有非query/evidence的fact inputs，将它们融合到边里：用一条新边代替这条边，新边相比旧边，删掉了所有fact inputs，其他inputs不变，同时，该边的概率变为原概率*这些fact inputs的概率。

我突然想起来，前面所有rewrite的逻辑都没有处理negation——边的语义永远和bodynegations绑定。求概率的时候，需要看对应input是否是negated，决定取反与否；生成边的时候，也同时要一致的生成 bodynegation信息。请你完整检查一下rewrite已有的所有逻辑，保证正确处理negation。

## engineering

图小的时候，如果必要bddManager，可以减少初始化内存吗？

全and子图的优化。

避免最终的forward comp，直接求query node的公式和概率的优化

detect and rewrite on the fly. 