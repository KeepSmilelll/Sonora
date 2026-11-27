#include "core/TextEncodingHelper.h"

#include <QtTest/QtTest>

using namespace virela;

class TestTextEncoding : public QObject {
    Q_OBJECT

private slots:
    void utf8AndBomDetection() {
        const QString expected = QStringLiteral("晴天 - 周杰伦 (FLAC 无损)");
        QByteArray utf8 = expected.toUtf8();
        QVERIFY(TextEncodingHelper::isValidUtf8(utf8));
        QCOMPARE(TextEncodingHelper::decodeBytes(utf8), expected);

        QByteArray withBom("\xEF\xBB\xBF", 3);
        withBom.append(utf8);
        QCOMPARE(TextEncodingHelper::decodeBytes(withBom), expected);
    }

    void gb18030BytesAutoDecode() {
        // "晴天" in GB18030/GBK: C7 E7 CC EC
        // "周杰伦" in GB18030/GBK: D6 DC BD DC C2 D7
        const QByteArray gbBytes = QByteArray::fromHex("C7E7CCEC202D20D6DCBDDCC2D7");
        QVERIFY(!TextEncodingHelper::isValidUtf8(gbBytes));
        const QString decoded = TextEncodingHelper::decodeBytes(gbBytes);
        QCOMPARE(decoded, QStringLiteral("晴天 - 周杰伦"));
    }

    void repairFakeLatin1GbkId3Mojibake() {
        // Simulate FFmpeg reading GBK bytes under ISO-8859-1 encoding into QString
        const QByteArray gbBytes = QByteArray::fromHex("D2B9C7FA"); // "夜曲" in GBK
        const QString mojibake = QString::fromLatin1(gbBytes);
        QCOMPARE(TextEncodingHelper::repairMetadataString(mojibake), QStringLiteral("夜曲"));
    }

    void preservesLegitimateEuropeanAccentsAndValidUnicode() {
        const QString french = QStringLiteral("Café del Mar");
        QCOMPARE(TextEncodingHelper::repairMetadataString(french), french);

        const QString icelandic = QStringLiteral("Sigur Rós - Björk");
        QCOMPARE(TextEncodingHelper::repairMetadataString(icelandic), icelandic);

        const QString cleanCjk = QStringLiteral("花海 - 魔杰座");
        QCOMPARE(TextEncodingHelper::repairMetadataString(cleanCjk), cleanCjk);
    }
};

QTEST_MAIN(TestTextEncoding)
#include "test_text_encoding.moc"
