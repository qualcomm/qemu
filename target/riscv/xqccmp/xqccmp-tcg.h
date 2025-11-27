static void emit_qc_cm_mva01s(DisasContext *ctx, TCGv_env env, uint8_t vi_18, uint8_t vi_13);
static void emit_qc_cm_mvsa01(DisasContext *ctx, TCGv_env env, uint8_t vi_19, uint8_t vi_14);
static void emit_qc_cm_pop(DisasContext *ctx, TCGv_env env, uint8_t vi_70, uint8_t vi_62);
static void emit_qc_cm_popret(DisasContext *ctx, TCGv_env env, uint8_t vi_71, uint8_t vi_63);
static void emit_qc_cm_popretz(DisasContext *ctx, TCGv_env env, uint8_t vi_72, uint8_t vi_64);
static void emit_qc_cm_push(DisasContext *ctx, TCGv_env env, uint8_t vi_66, uint8_t vi_9);
static void emit_qc_cm_pushfp(DisasContext *ctx, TCGv_env env, uint8_t vi_67, uint8_t vi_10);
int helper_to_tcg_dispatcher(void *func, TCGTemp *ret_temp, int nargs, TCGTemp **args);
