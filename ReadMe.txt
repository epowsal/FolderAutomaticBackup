
没做的：
git “bad index file sha1 signature fatal: index file corrupt”错误
标签： git
2013-08-24 14:09 2726人阅读 评论(0) 收藏 举报
 分类： git（1）  
版权声明：本文为博主原创文章，未经博主允许不得转载。

在执行commit或revert等操作时，提示“bad index file sha1 signature fatal: index file corrupt”错误，导致操作失败。这是由于git的index文件出错。需要删除.git/index文件，然后在仓库目录下运行git reset，重新生成index文件。
git reset还可以删除已经commit，但未push上去的信息。



2:
git “bad index file sha1 signature fatal: index file corrupt”错误
删除提示的hash文件就行了

