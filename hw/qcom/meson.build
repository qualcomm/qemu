subdir('cc')
subdir('graphics')

system_ss.add(when: 'CONFIG_QCOM_VIRT', if_true: files('logger.c'))
system_ss.add(when: 'CONFIG_QCOM_CRM_V2', if_true: files('crm-v2.c'))
system_ss.add(when: 'CONFIG_QCOM_CMD_DB', if_true: files('cmd-db.c'))
system_ss.add(when: 'CONFIG_QCOM_RPMH_RSC', if_true: files('rpmh-rsc.c'))
system_ss.add(when: 'CONFIG_QCOM_RPMH_CLK', if_true: files('rpmh-clk.c'))
system_ss.add(when: 'CONFIG_QCOM_ICC_RPMH', if_true: files('icc-rpmh.c'))
system_ss.add(when: 'CONFIG_QCOM_QMP', if_true: files('qmp.c'))
system_ss.add(when: 'CONFIG_QCOM_SMMU', if_true: files('franksmmu.c'))

specific_ss.add(when: 'CONFIG_QCOM_SMC', if_true: files('smc.c'))

