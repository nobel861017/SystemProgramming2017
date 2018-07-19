//range_cut
#include <stdio.h>
void assign_jobs(int num_of_clients){
	int range, pre = 96;
	range = 26/num_of_clients;
	for(int i = 0; i < num_of_clients; i++){
		int start = pre + 1;
		int end = (i != (num_of_clients-1) ? pre + range : 122);
		pre = end;
		printf("%d client get range %d ~ %d\n", i, start, end);
	}
}
int main(){
	int n;
	scanf("%d", &n);
	assign_jobs(n);
	return 0;
}