#include <platform_def.h>


#define GREEN_TEED_CORE_COUNT	PLATFORM_CORE_COUNT

#define GREEN_TEED_SP_CONTEXT_X19    8*0
#define GREEN_TEED_SP_CONTEXT_X20    8*1
#define GREEN_TEED_SP_CONTEXT_X21    8*2
#define GREEN_TEED_SP_CONTEXT_X22    8*3
#define GREEN_TEED_SP_CONTEXT_X23    8*4
#define GREEN_TEED_SP_CONTEXT_X24    8*5
#define GREEN_TEED_SP_CONTEXT_X25    8*6
#define GREEN_TEED_SP_CONTEXT_X26    8*7
#define GREEN_TEED_SP_CONTEXT_X27    8*8
#define GREEN_TEED_SP_CONTEXT_X28    8*9
#define GREEN_TEED_SP_CONTEXT_X29    8*10
#define GREEN_TEED_SP_CONTEXT_X30    8*11
#define GREEN_TEED_SP_CONTEXT_SIZE   8*12

#ifndef __ASSEMBLER__
#include <stdint.h>
#include <context.h>

#define GREEN_TEE_STATE_OFF			0
#define GREEN_TEE_STATE_ON			1
#define GREEN_TEE_STATE_SUSPEND		2
#define GREEN_TEE_STATE_UNKNOWN		3

typedef struct{
	uint64_t mpidr_el1;
	uint64_t stack;
	cpu_context_t cpu_context;
	uint8_t state;
} green_tee_cpu_context_t;

typedef struct green_tee_vector_table{
   	uint64_t yield_smc_entry;
	uint64_t fast_smc_entry;
	uint64_t cpu_on_entry;
	uint64_t cpu_off_entry;
	uint64_t cpu_resume_entry;
	uint64_t cpu_suspend_entry;
	uint64_t fiq_entry;
	uint64_t system_off_entry;
	uint64_t system_reset_entry;

} green_tee_vector_table_t;

struct green_tee_print_data{
	char* str; // userspace address
	int len;
};
void green_teed_init_ep_state(struct entry_point_info *green_tee_entry_point,
		        uint64_t pc, uint64_t arg0,
				uint64_t arg1, uint64_t arg2, uint64_t arg3,
				green_tee_cpu_context_t *green_tee_ctx);
uint64_t green_teed_enter_sp(uint64_t* stack);
uint64_t green_teed_exit_sp(uint64_t stack);
void green_tee_init_vector_table(uint64_t vbar_el1);
int32_t green_teed_synchronous_sp_entry(green_tee_cpu_context_t* context);
int32_t green_teed_synchronous_sp_exit(green_tee_cpu_context_t* context);

extern const spd_pm_ops_t green_teed_pm;

#endif  /* __ASSEMBLER__ */
