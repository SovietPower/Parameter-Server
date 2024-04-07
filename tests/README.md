
## 使用 PS 进行分布式 LR 训练

以使用梯度下降进行逻辑回归的训练为例。

演示中的并行仅为处理样本、计算梯度时的并行（数据并行），不涉及参数更新的并行（模型并行），这需要将特征分布到多个 server 上，但此处数据集只有123个特征所以没什么影响。

### 启动

需要设置的额外参数（在 local.py 中修改，避免在命令行中设太多参数）：

- `DATA_DIR`：样本数据所在父目录和模型的输出位置。
- `NUM_FEATURE`：特征数量。
- `BATCH_SIZE`：每轮训练的批次大小。默认 -1 为 BGD。
- `ITERATION`：总训练轮数。
- `TEST_PERIOD`：每隔多少轮，进行一次正确率测试。默认为0，仅在最后进行。

- `SYNC_MODE`：同步模式。0：同步；1：异步。
- `LEARNING_RATE`：学习率。
- `C`：正则化系数。

可选参数：

- `USE_OLD_MODEL`：是否使用已有模型继续训练。默认不使用。如果使用，则通过该参数指定模型文件名称（完整路径为`DATA_DIR/model/USE_OLD_MODEL`）。
- `USE_ADAM`：是否使用 Adam 优化学习率。默认不使用，设为任意值启用。
- `USE_KEY_CACHING`：是否使用 key caching 优化。默认不使用，设为任意值启用。

设置完参数后，运行：

```bash
# 使用 PS 进行逻辑回归
python .\local.py -ns=1 -nw=4 -exec='.\exe\LR_ps.exe' -lr

# 不使用 PS 进行逻辑回归（ns, nw 设置多少无所谓，但是是必需参数）
python .\local.py -ns=0 -nw=0 -exec='.\exe\LR_normal.exe' -lr_normal
```

将从`DATA_DIR/train`中读取训练数据集，从`DATA_DIR/test`中读取测试数据集。

最终的模型参数与每个 worker 本地的模型参数会输出到`DATA_DIR/model`下，格式为：

```
模型迭代次数
特征数量
特征...
```

注：不使用多 customer。

### 介绍

**数据集**

训练集为UCI Machine Learning Repository中的Adult数据集，是一个关于人口普查收入数据的数据集，目标是根据个人特征预测其年收入是否超过50,000美元，是一个典型的二分类任务。它包含年龄、工作类型、受教育程度等14个特征，共计48842个样本，并以3：1的比例分为训练集和测试集。
数据处理方式与[LIBSVM a9a](https://www.csie.ntu.edu.tw/~cjlin/libsvmtools/datasets/)相同，每条数据从原始特征的14个转换成123个。

在使用 PS 进行分布式训练时，训练集会被分成四份，供四个 worker 同时进行训练。
当每个 worker 在自己的训练集上完成一轮迭代并上传梯度后，整个系统完成一轮迭代。

**测试结果**

本地训练时，使用4个worker与1个server进行分布式训练中模型达到相同精确度所需的时间几乎是单机训练的1/4。

不过即使使用完全串行和批量梯度下降，分布式训练与单机训练的训练效果也不完全相同，可能是哪里有点问题。



