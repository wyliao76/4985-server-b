#include <cgreen/cgreen.h>
#include "messaging.h"
#include <string.h>

Describe(serializer);

BeforeEach(serializer)
{
}

AfterEach(serializer)
{
}

Ensure(serializer, return_strcut)
{
    unsigned char buf[6] = {
        0x0A,    // 10
        0x01,    // 1
        0x00,    // 0
        0x00,    // 0
        0x00,    // 0
        0x16,    // 22
    };
    header_t header;
    response_t response;

    deserialize_header(&header, &response, buf, sizeof(buf));

    assert_that(header.type, is_equal_to(10));
    assert_that(header.version, is_equal_to(1));
    assert_that(header.sender_id, is_equal_to(0));
    assert_that(header.payload_len, is_equal_to(22));
}

Ensure(serializer, fail_no_header)
{
    unsigned char     buf[0] = {};
    header_t header;
    response_t response;

    assert_that(deserialize_header(&header, &response, buf, sizeof(buf)), is_equal_to(-1));
}

TestSuite *tests()
{
    TestSuite *suite = create_test_suite();
    add_test_with_context(suite, serializer, return_strcut);
    add_test_with_context(suite, serializer, fail_no_header);
    return suite;
}

int main(int argc, char **argv)
{
    return run_test_suite(tests(), create_text_reporter());
}
