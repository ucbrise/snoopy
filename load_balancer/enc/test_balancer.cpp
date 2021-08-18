#include <stdlib.h>

#include "balancer.h"
#include "test_balancer.h"

using namespace std;

void test_balancer() {
    int num_suborams = 10;
    int num_reqs = 100;
    int num_blocks = 2048;
    int total_out_reqs = num_suborams * num_reqs;
    printf("Going to create load balancer\n");


    LoadBalancer lb(num_suborams, num_blocks, 1);
    printf("Going to create batch\n");
    Batch batch = Batch(num_reqs);
    printf("Created batch\n");
    uint8_t block[BLOCK_LEN];
    for (int i = 0; i < num_reqs; i++) {
        batch.add_incoming_request(KeyBlockPairBucketItem(i, i, block));
    }
    printf("Created %d requests\n", num_reqs);

    lb.create_outgoing_batch(batch);
    printf("Finished batch with %d subORAMs and %d reqs\n", num_suborams, num_reqs);
    for (int i = 0; i < batch.outgoing_reqs.size(); i++) {
        printf("%d/%zu: Req %d assigned to %d\n", i+1, batch.outgoing_reqs.size(), batch.outgoing_reqs[i].item.req.key, batch.outgoing_reqs[i].item.SID);
    }
    printf("Done with testing load balancer\n");
}

int main() {
    test_balancer();
}
