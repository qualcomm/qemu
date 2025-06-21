subdir('graphics')

system_ss.add(when: 'CONFIG_QCOM_VIRT', if_true: files('logger.c'))
system_ss.add(when: 'CONFIG_QCOM_CRM_V2', if_true: files('crm-v2.c'))
system_ss.add(when: 'CONFIG_QCOM_CMD_DB', if_true: files('cmd-db.c'))
