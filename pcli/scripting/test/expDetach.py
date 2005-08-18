# expDetach [ <expId_spec> ] [ <target_list> ] [ <expType_list> ]

import oss

my_id = oss.ExpId(oss.expCreate())

my_host = oss.HostList()
my_host += ["bimbo","bozo"]

my_rank = oss.RankList()
my_rank += [1,3,(22,33),564]

my_cluster = oss.ClusterList("cluster_1")

my_thread = oss.ThreadList(987)
my_thread += 765

my_file = oss.FileList("file_1")

my_pid = oss.PidList()
my_pid.add((1,25))

my_exptype = oss.ExpTypeList()
my_exptype += "pcsamp"
my_exptype.add("usertime")

# The number here is hard coded in a lowlevel routine
oss.expDetach(my_id,my_host,my_rank,my_cluster,my_thread,my_file,my_pid,my_exptype)

oss.expDetach(my_id,my_pid,my_exptype)
oss.expDetach(my_id,my_pid)
oss.expDetach(my_id,my_exptype)
oss.expDetach(my_id)
oss.expDetach(my_pid,my_exptype)
oss.expDetach(my_pid)
oss.expDetach(my_exptype)
oss.expDetach()
