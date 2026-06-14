#include "Texture/NumericTexture.h"
#include "Rendering/Renderer.h"
#include "Texture/Bitmap.h"
#include <filesystem>
#include <stdexcept>

NumericTexture::NumericTexture(std::vector<std::string> numericsFiles)
{
    if (numericsFiles.size() != 10) {
        throw std::runtime_error("NumericTexture::NumericTexture: numericsFiles.size() != 10");
    }

    Position2 = UDim2::fromOffset(0, 0);
    AnchorPoint = { 0, 0 };

    m_numericsTexture.resize(10);
    for (int i = 0; i < 10; i++) {
        auto path = numericsFiles[i];
        m_numericsTexture[i] = new Texture2D(path);
        m_numbericsWidth[i] = m_numericsTexture[i]->GetOriginalRECT();
        if (m_numbericsWidth[i].right > m_maxWidth) {
            m_maxWidth = m_numbericsWidth[i].right;
        }
    }
}

NumericTexture::NumericTexture(std::vector<std::filesystem::path> numericsPath)
{
    if (numericsPath.size() != 10) {
        throw std::runtime_error("NumericTexture::NumericTexture: numericsFiles.size() != 10");
    }

    Position2 = UDim2::fromOffset(0, 0);
    AnchorPoint = { 0, 0 };

    m_numericsTexture.resize(10);
    for (int i = 0; i < 10; i++) {
        auto path = numericsPath[i];
        m_numericsTexture[i] = new Texture2D(path);
        m_numbericsWidth[i] = m_numericsTexture[i]->GetOriginalRECT();
        if (m_numbericsWidth[i].right > m_maxWidth) {
            m_maxWidth = m_numbericsWidth[i].right;
        }
    }
}

NumericTexture::~NumericTexture()
{
    for (auto &tex : m_numericsTexture) {
        delete tex;
    }
}

void NumericTexture::DrawNumber(int number)
{
    GameWindow *window = GameWindow::GetInstance();

    std::string numberString = std::to_string(number);
    if (MaxDigits != 0 && numberString.size() > MaxDigits) {
        numberString = numberString.substr(numberString.size() - MaxDigits, MaxDigits);
    } else {
        while (numberString.size() < MaxDigits && FillWithZeros) {
            numberString = "0" + numberString;
        }
    }

    LONG xPos = static_cast<LONG>(window->GetBufferWidth() * Position.X.Scale) + static_cast<LONG>(Position.X.Offset);
    LONG yPos = static_cast<LONG>(window->GetBufferHeight() * Position.Y.Scale) + static_cast<LONG>(Position.Y.Offset);

    LONG xMPos = static_cast<LONG>(window->GetBufferWidth() * Position2.X.Scale) + static_cast<LONG>(Position2.X.Offset);
    LONG yMPos = static_cast<LONG>(window->GetBufferHeight() * Position2.Y.Scale) + static_cast<LONG>(Position2.Y.Offset);

    xPos += xMPos;
    yPos += yMPos;

    float offsetScl = (float)Offset / 100.0f;

    switch (NumberPosition) {
        case NumericPosition::LEFT:
        {
            int tx = xPos;
            for (int i = (int)numberString.length() - 1; i >= 0; i--) {
                int digit = numberString[i] - '0';

                tx -= m_maxWidth + (int)(m_maxWidth * offsetScl);
                auto tex = m_numericsTexture[digit];
                float centeredX = tx + (m_maxWidth - m_numbericsWidth[digit].right) / 2.0f;
                tex->Position = UDim2({ 0, centeredX }, { 0, (float)yPos });
                tex->AlphaBlend = AlphaBlend;
                tex->AnchorPoint = AnchorPoint;
                tex->Draw();
            }
            break;
        }

        case NumericPosition::MID:
        {
            int totalWidth = 0;
            for (int i = 0; i < numberString.length(); i++) {
                totalWidth += m_maxWidth + (int)(m_maxWidth * offsetScl);
            }

            int tx = xPos - totalWidth / 2 + (Offset * totalWidth) / 200;
            for (int i = 0; i < numberString.length(); i++) {
                int  digit = numberString[i] - '0';
                auto tex = m_numericsTexture[digit];
                float centeredX = tx + (m_maxWidth - m_numbericsWidth[digit].right) / 2.0f;
                tex->Position = UDim2({ 0, centeredX }, { 0, (float)yPos });
                tex->AlphaBlend = AlphaBlend;
                tex->AnchorPoint = AnchorPoint;
                tex->Draw();
                tx += m_maxWidth + (int)(m_maxWidth * offsetScl);
            }
            break;
        }

        case NumericPosition::RIGHT:
        {
            int tx = xPos;
            for (int i = 0; i < numberString.length(); i++) {
                int  digit = numberString[i] - '0';
                auto tex = m_numericsTexture[digit];
                float centeredX = tx + (m_maxWidth - m_numbericsWidth[digit].right) / 2.0f;
                tex->Position = UDim2({ 0, centeredX }, { 0, (float)yPos });
                tex->AnchorPoint = AnchorPoint;
                tex->AlphaBlend = AlphaBlend;
                tex->Draw();
                tx += m_maxWidth + (int)(m_maxWidth * offsetScl);
            }
            break;
        }

        default:
        {
            throw std::runtime_error("Invalid NumericPosition");
        }
    }
}

void NumericTexture::SetValue(int value)
{ // Add SetValueinto DrawNumber for NumericTexture so it can update
    DrawNumber(value);
}

NumericPosition IntToPos(int i)
{
    switch (i) {
        case -1:
            return NumericPosition::LEFT;
        case 0:
            return NumericPosition::MID;
        case 1:
            return NumericPosition::RIGHT;

        default:
            throw std::runtime_error("IntToPos: i is not a valid NumericPosition");
    }
}
