#include "c_api_test/c_api_test.h"

using namespace kuzu::common;
using namespace kuzu::main;
using namespace kuzu::testing;

class CApiFlatTupleTest : public CApiTest {
public:
    std::string getInputDir() override {
        return TestHelper::appendKuzuRootPath("dataset/tinysnb/");
    }
};

TEST_F(CApiFlatTupleTest, GetValue) {
    kuzu_query_result result;
    kuzu_flat_tuple flatTuple;
    kuzu_state state;
    auto connection = getConnection();
    state = kuzu_connection_query(connection,
        "MATCH (a:person) RETURN a.fName, a.age, a.height ORDER BY a.fName LIMIT 1", &result);
    ASSERT_EQ(state, KuzuSuccess);
    ASSERT_TRUE(kuzu_query_result_is_success(&result));
    state = kuzu_query_result_get_next(&result, &flatTuple);
    ASSERT_EQ(state, KuzuSuccess);
    kuzu_value value;
    ASSERT_EQ(kuzu_flat_tuple_get_value(&flatTuple, 0, &value), KuzuSuccess);
    ASSERT_NE(value._value, nullptr);
    auto valueCpp = static_cast<Value*>(value._value);
    ASSERT_NE(valueCpp, nullptr);
    ASSERT_EQ(valueCpp->getDataType().getLogicalTypeID(), LogicalTypeID::STRING);
    ASSERT_EQ(valueCpp->getValue<std::string>(), "Alice");
    kuzu_value_destroy(&value);
    ASSERT_EQ(kuzu_flat_tuple_get_value(&flatTuple, 1, &value), KuzuSuccess);
    ASSERT_NE(value._value, nullptr);
    valueCpp = static_cast<Value*>(value._value);
    ASSERT_NE(valueCpp, nullptr);
    ASSERT_EQ(valueCpp->getDataType().getLogicalTypeID(), LogicalTypeID::INT64);
    ASSERT_EQ(valueCpp->getValue<int64_t>(), 35);
    kuzu_value_destroy(&value);
    ASSERT_EQ(kuzu_flat_tuple_get_value(&flatTuple, 2, &value), KuzuSuccess);
    ASSERT_NE(value._value, nullptr);
    valueCpp = static_cast<Value*>(value._value);
    ASSERT_NE(valueCpp, nullptr);
    ASSERT_EQ(valueCpp->getDataType().getLogicalTypeID(), LogicalTypeID::FLOAT);
    ASSERT_FLOAT_EQ(valueCpp->getValue<float>(), 1.731);
    kuzu_value_destroy(&value);
    ASSERT_EQ(kuzu_flat_tuple_get_value(&flatTuple, 222, &value), KuzuError);
    kuzu_flat_tuple_destroy(&flatTuple);
    kuzu_query_result_destroy(&result);
}

TEST_F(CApiFlatTupleTest, ToString) {
    kuzu_query_result result;
    kuzu_flat_tuple flatTuple;
    kuzu_state state;
    auto connection = getConnection();
    state = kuzu_connection_query(connection,
        "MATCH (a:person) RETURN a.fName, a.age, a.height ORDER BY a.fName LIMIT 1", &result);
    ASSERT_EQ(state, KuzuSuccess);
    ASSERT_TRUE(kuzu_query_result_is_success(&result));
    state = kuzu_query_result_get_next(&result, &flatTuple);
    ASSERT_EQ(state, KuzuSuccess);
    auto columnWidths = (uint32_t*)malloc(3 * sizeof(uint32_t));
    columnWidths[0] = 10;
    columnWidths[1] = 5;
    columnWidths[2] = 10;
    char* str = kuzu_flat_tuple_to_string(&flatTuple);
    ASSERT_EQ(std::string(str), "Alice|35|1.731000\n");
    kuzu_destroy_string(str);
    free(columnWidths);
    kuzu_flat_tuple_destroy(&flatTuple);
    kuzu_query_result_destroy(&result);
}
