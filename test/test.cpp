#include <gtest/gtest.h>
#include <my_malloc.h>

TEST(MallocTest, InitAndCheckAvailableMemory) {
  reset_heap();
  size_t size = available_memory();
  ASSERT_EQ(size, HEAP_SIZE - sizeof(node_t));
}

TEST(MallocTest, CheckNumberOfFreeNodes) {
  reset_heap();
  int count = number_of_free_nodes();
  ASSERT_EQ(count, 1);
}

TEST(MallocTest, SimpleMallocCall) {
  reset_heap();
  void *p = my_malloc(100);
  ASSERT_TRUE(p != NULL);
}

TEST(MallocTest, SimpleMallocCallNumberOfFreeNodes) {
  reset_heap();
  void *p = my_malloc(100);
  ASSERT_TRUE(p != NULL);
  int count = number_of_free_nodes();
  ASSERT_EQ(count, 1);
}

TEST(MallocTest, MallocTooMuchMemory) {
  reset_heap();
  void *p = my_malloc(10000);
  ASSERT_TRUE(p == NULL);
}

TEST(MallocTest, SimpleMallocCallCheckAvailableMemory) {
  reset_heap();
  void *p = my_malloc(100);
  ASSERT_TRUE(p != NULL);
  size_t size = available_memory();
  ASSERT_EQ(size, HEAP_SIZE - sizeof(node_t) * 2 - 100);
}

TEST(MallocTest, CheckYourHead) {
  reset_heap();
  void *p = my_malloc(100);
  ASSERT_FALSE(p == NULL);

  header_t *head = (header_t *)((char *)p - sizeof(header_t));
  ASSERT_EQ(head->magic, MAGIC);
}

TEST(MallocTest, SimpleFreeCheck) {
  reset_heap();
  void *p = my_malloc(100);
  ASSERT_FALSE(p == NULL);

  void *k = my_malloc(100);
  ASSERT_FALSE(k == NULL);
  my_free(p);
  ASSERT_EQ(number_of_free_nodes(), 2);
  k = NULL;  // to stop compiler warnings.
}


int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
