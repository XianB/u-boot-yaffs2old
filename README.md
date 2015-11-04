# u-boot with yaffs2

该项目是为了将**yaffs2移植到ucos操作系统**上，但是为了实现该功能，目前需要先将yaffs2移植到u-boot中，随后将**yaffs2连同mtd层nand驱动层**一起抽取出来从而实现一个**链接库**，供操作系统链接使用
